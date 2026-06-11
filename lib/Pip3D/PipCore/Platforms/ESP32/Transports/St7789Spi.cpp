#include <PipCore/Config/Features.hpp>

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789

#if PIPCORE_TARGET_ESP32
#include <esp_heap_caps.h>
#include <esp_attr.h>
#if __has_include(<esp_memory_utils.h>)
#include <esp_memory_utils.h>
#endif
#if __has_include(<soc/soc_memory_layout.h>)
#include <soc/soc_memory_layout.h>
#endif
#include <hal/gpio_ll.h>
#endif

#include <PipCore/Debug/MemoryHooks.hpp>
#include <PipCore/Platforms/ESP32/Transports/St7789Spi.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_rom_gpio.h>
#include <esp_rom_sys.h>
#include <esp_heap_caps.h>
#include <algorithm>

namespace pipcore::esp32
{
    namespace
    {
        [[nodiscard]] inline constexpr bool isPinValid(int8_t pin) noexcept { return pin >= 0; }

#if PIPCORE_TARGET_ESP32
        static DRAM_ATTR volatile uint8_t g_lastDcLevel = 0xFF;
#else
        static volatile uint8_t g_lastDcLevel = 0xFF;
#endif

        [[nodiscard]] inline bool isDmaCapable(const void *p) noexcept
        {
#if PIPCORE_TARGET_ESP32
#if defined(CONFIG_IDF_TARGET_ESP32S3)
            return esp_ptr_internal(p) || esp_ptr_dma_ext_capable(p);
#else
            return esp_ptr_dma_capable(p);
#endif
#else
            (void)p;
            return false;
#endif
        }

        inline void IRAM_ATTR gpio_fast_write_high(int8_t pin) noexcept
        {
#if PIPCORE_TARGET_ESP32
            gpio_ll_set_level(&GPIO, static_cast<gpio_num_t>(pin), 1);
#else
            (void)pin;
#endif
        }

        inline void IRAM_ATTR gpio_fast_write_low(int8_t pin) noexcept
        {
#if PIPCORE_TARGET_ESP32
            gpio_ll_set_level(&GPIO, static_cast<gpio_num_t>(pin), 0);
#else
            (void)pin;
#endif
        }

        void IRAM_ATTR lcd_spi_pre_cb(spi_transaction_t *t)
        {
#if PIPCORE_TARGET_ESP32
            uint32_t packed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t->user));
            int8_t pin = static_cast<int8_t>(static_cast<uint8_t>((packed >> 8) & 0xFF));
            uint8_t level = static_cast<uint8_t>(packed & 0xFF);

            if (pin >= 0 && level != g_lastDcLevel)
            {
                gpio_ll_set_level(&GPIO, static_cast<gpio_num_t>(pin), level);
                g_lastDcLevel = level;
            }
#else
            (void)t;
#endif
        }

        [[nodiscard]] inline void *packDcInfo(int8_t pin, uint8_t level) noexcept
        {
            uint32_t packed = (static_cast<uint32_t>(static_cast<uint8_t>(pin)) << 8) | level;
            return reinterpret_cast<void *>(static_cast<uintptr_t>(packed));
        }

        [[nodiscard]] constexpr int8_t resolveDefaultMosi() noexcept
        {
#if defined(CONFIG_IDF_TARGET_ESP32)
            return 13;
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
            return 11;
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
            return 7;
#else
            return -1;
#endif
        }

        [[nodiscard]] constexpr int8_t resolveDefaultSclk() noexcept
        {
#if defined(CONFIG_IDF_TARGET_ESP32)
            return 14;
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
            return 12;
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
            return 6;
#else
            return -1;
#endif
        }

        [[nodiscard]] constexpr int8_t resolveDefaultCs() noexcept
        {
#if defined(CONFIG_IDF_TARGET_ESP32)
            return 15;
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
            return 10;
#else
            return -1;
#endif
        }
    }

    void St7789Spi::configure(int8_t mosi, int8_t sclk, int8_t cs, int8_t dc, int8_t rst, uint32_t hz) noexcept
    {
        deinit();
        _pinMosi = mosi;
        _pinSclk = sclk;
        _pinCs = cs;
        _pinDc = dc;
        _pinRst = rst;
        _hz = hz ? hz : 80000000U;
        _spiHandle = nullptr;
        _dmaBuf[0] = nullptr;
        _dmaBuf[1] = nullptr;
        _busAcquired = false;
        _initialized = false;
        _lastError = st7789::IoError::None;

        for (int i = 0; i < MaxAsyncTrans; ++i)
        {
            _asyncTrans[i] = spi_transaction_t{};
        }
        _asyncNext = 0;
        _asyncInFlight = 0;

        if (!isPinValid(_pinMosi))
            _pinMosi = resolveDefaultMosi();
        if (!isPinValid(_pinSclk))
            _pinSclk = resolveDefaultSclk();
        if (!isPinValid(_pinCs))
            _pinCs = resolveDefaultCs();
    }

    St7789Spi::~St7789Spi() { deinit(); }

    bool St7789Spi::fail(st7789::IoError error)
    {
        _lastError = error;
        return false;
    }

    bool St7789Spi::init()
    {
        clearError();
        if (_initialized)
            return true;
        if (!isPinValid(_pinDc))
            return fail(st7789::IoError::InvalidConfig);
        if (!initSpi())
            return false;

        gpio_config_t io{};
        io.intr_type = GPIO_INTR_DISABLE;
        io.mode = GPIO_MODE_OUTPUT;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.pull_up_en = GPIO_PULLUP_DISABLE;
        io.pin_bit_mask = 1ULL << (uint8_t)_pinDc;
        if (isPinValid(_pinRst))
            io.pin_bit_mask |= 1ULL << (uint8_t)_pinRst;
        if (gpio_config(&io) != ESP_OK)
        {
            deinit();
            return fail(st7789::IoError::Gpio);
        }

        _initialized = true;
        return true;
    }

    void St7789Spi::deinit()
    {
        if (_spiHandle)
        {
            (void)waitComplete();
            spi_bus_remove_device((spi_device_handle_t)_spiHandle);
            spi_bus_free(SPI2_HOST);
            _spiHandle = nullptr;
        }

        for (int i = 0; i < 2; ++i)
        {
            if (_dmaBuf[i])
            {
                void *freed = _dmaBuf[i];
                heap_caps_free(_dmaBuf[i]);
                _dmaBuf[i] = nullptr;
                pipcore::debug::memoryEvent(pipcore::debug::MemoryEvent::Free, "st7789.dma.free", freed, nullptr, DmaBufferBytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
            }
        }

        _busAcquired = false;
        _initialized = false;
        _asyncNext = 0;
        _asyncInFlight = 0;
        g_lastDcLevel = 0xFF;
    }

    bool St7789Spi::initSpi()
    {
        if (_spiHandle)
            return true;
        if (!isPinValid(_pinMosi) || !isPinValid(_pinSclk))
            return fail(st7789::IoError::InvalidConfig);

        spi_bus_config_t bus{};
        bus.mosi_io_num = _pinMosi;
        bus.miso_io_num = -1;
        bus.sclk_io_num = _pinSclk;
        bus.quadwp_io_num = -1;
        bus.quadhd_io_num = -1;
        bus.max_transfer_sz = (int)HardwareMaxDmaBytes;

        if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK)
            return fail(st7789::IoError::SpiBusInit);

        spi_device_interface_config_t dev{};
        dev.mode = 3;
        dev.clock_speed_hz = (int)_hz;
        dev.spics_io_num = isPinValid(_pinCs) ? _pinCs : -1;
        dev.queue_size = MaxAsyncTrans;
        dev.cs_ena_pretrans = 0;
        dev.cs_ena_posttrans = 0;
        dev.flags = SPI_DEVICE_NO_DUMMY | SPI_DEVICE_3WIRE;
        dev.pre_cb = lcd_spi_pre_cb;

        spi_device_handle_t h = nullptr;
        if (spi_bus_add_device(SPI2_HOST, &dev, &h) != ESP_OK)
        {
            spi_bus_free(SPI2_HOST);
            return fail(st7789::IoError::SpiDeviceAdd);
        }
        _spiHandle = h;

        for (int i = 0; i < 2; ++i)
        {
            _dmaBuf[i] = static_cast<uint8_t *>(heap_caps_aligned_alloc(4, DmaBufferBytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
            pipcore::debug::memoryEvent(_dmaBuf[i] ? pipcore::debug::MemoryEvent::Alloc : pipcore::debug::MemoryEvent::AllocFail,
                                        "st7789.dma.alloc", _dmaBuf[i], nullptr, DmaBufferBytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        }

        if (!_dmaBuf[0] || !_dmaBuf[1])
        {
            deinit();
            return fail(st7789::IoError::DmaBufferAlloc);
        }

        for (int i = 0; i < MaxAsyncTrans; ++i)
        {
            _asyncTrans[i] = spi_transaction_t{};
        }

        _busAcquired = false;
        _asyncNext = 0;
        _asyncInFlight = 0;
        clearError();
        return true;
    }

    bool St7789Spi::setRst(bool level)
    {
        if (isPinValid(_pinRst))
        {
            if (level)
                gpio_fast_write_high(_pinRst);
            else
                gpio_fast_write_low(_pinRst);
        }
        return true;
    }

    void St7789Spi::delayMs(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

    bool IRAM_ATTR St7789Spi::writeCommand(uint8_t cmd)
    {
        if (!_spiHandle)
            return fail(st7789::IoError::NotReady);

        if (_asyncInFlight > 0)
        {
            if (!drainQueue())
                return false;
        }

        spi_transaction_t t{};
        t.flags = SPI_TRANS_USE_TXDATA;
        t.length = 8;
        t.tx_data[0] = cmd;
        t.user = packDcInfo(_pinDc, 0);

        if (spi_device_polling_transmit((spi_device_handle_t)_spiHandle, &t) != ESP_OK)
            return fail(st7789::IoError::CommandTransmit);
        return true;
    }

    bool IRAM_ATTR St7789Spi::write(const void *data, size_t len)
    {
        if (!len || !_spiHandle)
            return fail(st7789::IoError::NotReady);

        if (_asyncInFlight > 0)
        {
            if (!drainQueue())
                return false;
        }

        spi_transaction_t t{};
        t.user = packDcInfo(_pinDc, 1);
        if (len <= 4U)
        {
            t.flags = SPI_TRANS_USE_TXDATA;
            t.length = (int)(len * 8U);
            std::memcpy(t.tx_data, data, len);
        }
        else
        {
            t.length = (int)(len * 8U);
            t.tx_buffer = data;
        }

        if (spi_device_polling_transmit((spi_device_handle_t)_spiHandle, &t) != ESP_OK)
            return fail(st7789::IoError::DataTransmit);
        return true;
    }

    bool IRAM_ATTR St7789Spi::acquireBus()
    {
        if (!_spiHandle)
            return fail(st7789::IoError::NotReady);
        if (_busAcquired)
            return true;
        if (spi_device_acquire_bus(static_cast<spi_device_handle_t>(_spiHandle), portMAX_DELAY) != ESP_OK)
            return fail(st7789::IoError::QueueTransmit);
        _busAcquired = true;
        g_lastDcLevel = 0xFF;
        return true;
    }

    void IRAM_ATTR St7789Spi::releaseBus()
    {
        if (!_spiHandle || !_busAcquired)
            return;
        spi_device_release_bus(static_cast<spi_device_handle_t>(_spiHandle));
        _busAcquired = false;
    }

    bool St7789Spi::waitOldest()
    {
        if (_asyncInFlight <= 0)
            return true;

        spi_transaction_t *r = nullptr;
        esp_err_t err = spi_device_get_trans_result(static_cast<spi_device_handle_t>(_spiHandle), &r, portMAX_DELAY);
        if (err != ESP_OK || !r)
        {
            _asyncNext = 0;
            _asyncInFlight = 0;
            return fail(st7789::IoError::QueueResult);
        }

        _asyncInFlight--;
        return true;
    }

    bool IRAM_ATTR St7789Spi::drainQueue()
    {
        if (!_spiHandle)
            return true;

        bool success = true;

        while (_asyncInFlight > 0)
        {
            spi_transaction_t *r = nullptr;
            esp_err_t err = spi_device_get_trans_result(static_cast<spi_device_handle_t>(_spiHandle), &r, portMAX_DELAY);
            if (err != ESP_OK || !r)
            {
                success = false;
            }
            _asyncInFlight--;
        }

        _asyncNext = 0;
        _asyncInFlight = 0;

        return success;
    }

    bool IRAM_ATTR St7789Spi::writeAddrWindow(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye)
    {
        if (!_spiHandle)
            return fail(st7789::IoError::NotReady);

        if (_asyncInFlight > 0)
        {
            if (!drainQueue())
                return false;
        }

        if (!acquireBus())
            return false;

        spi_transaction_t t[5]{};

        t[0].flags = SPI_TRANS_USE_TXDATA;
        t[0].length = 8;
        t[0].tx_data[0] = 0x2A;
        t[0].user = packDcInfo(_pinDc, 0);

        t[1].flags = SPI_TRANS_USE_TXDATA;
        t[1].length = 32;
        t[1].tx_data[0] = static_cast<uint8_t>(xs >> 8);
        t[1].tx_data[1] = static_cast<uint8_t>(xs & 0xFF);
        t[1].tx_data[2] = static_cast<uint8_t>(xe >> 8);
        t[1].tx_data[3] = static_cast<uint8_t>(xe & 0xFF);
        t[1].user = packDcInfo(_pinDc, 1);

        t[2].flags = SPI_TRANS_USE_TXDATA;
        t[2].length = 8;
        t[2].tx_data[0] = 0x2B;
        t[2].user = packDcInfo(_pinDc, 0);

        t[3].flags = SPI_TRANS_USE_TXDATA;
        t[3].length = 32;
        t[3].tx_data[0] = static_cast<uint8_t>(ys >> 8);
        t[3].tx_data[1] = static_cast<uint8_t>(ys & 0xFF);
        t[3].tx_data[2] = static_cast<uint8_t>(ye >> 8);
        t[3].tx_data[3] = static_cast<uint8_t>(ye & 0xFF);
        t[3].user = packDcInfo(_pinDc, 1);

        t[4].flags = SPI_TRANS_USE_TXDATA;
        t[4].length = 8;
        t[4].tx_data[0] = 0x2C;
        t[4].user = packDcInfo(_pinDc, 0);

        spi_device_handle_t handle = static_cast<spi_device_handle_t>(_spiHandle);
        for (int i = 0; i < 5; ++i)
        {
            if (spi_device_polling_transmit(handle, &t[i]) != ESP_OK)
                return fail(st7789::IoError::CommandTransmit);
        }

        return true;
    }

    bool IRAM_ATTR St7789Spi::writePixelsImpl(const void *data, size_t len, bool useDmaBufferIfNonCapable)
    {
        if (!len || !_spiHandle)
            return fail(st7789::IoError::NotReady);

        if (!acquireBus())
            return false;

        const uint8_t *p = static_cast<const uint8_t *>(data);
        size_t remaining = len;

        const bool directDma = isDmaCapable(p) && ((reinterpret_cast<uintptr_t>(p) & 3U) == 0U);

        if (directDma || !useDmaBufferIfNonCapable)
        {
            while (remaining > 0)
            {
                while (_asyncInFlight >= MaxAsyncTrans)
                {
                    if (!waitOldest())
                        return false;
                }

                const size_t chunk = std::min(remaining, HardwareMaxDmaBytes);
                spi_transaction_t *t = &_asyncTrans[_asyncNext];

                t->flags = (remaining > chunk && isPinValid(_pinCs)) ? SPI_TRANS_CS_KEEP_ACTIVE : 0;
                t->length = static_cast<int>(chunk * 8U);
                t->rxlength = 0;
                t->tx_buffer = p;
                t->rx_buffer = nullptr;
                t->user = packDcInfo(_pinDc, 1);

                esp_err_t err = spi_device_queue_trans(static_cast<spi_device_handle_t>(_spiHandle), t, portMAX_DELAY);
                if (err != ESP_OK)
                    return fail(st7789::IoError::QueueTransmit);

                _asyncNext = (_asyncNext + 1) % MaxAsyncTrans;
                _asyncInFlight++;

                p += chunk;
                remaining -= chunk;
            }
            return true;
        }
        else
        {
            if (!_dmaBuf[0] || !_dmaBuf[1])
                return fail(st7789::IoError::NotReady);

            while (remaining > 0)
            {
                while (_asyncInFlight >= MaxDmaBufs)
                {
                    if (!waitOldest())
                        return false;
                }

                const int slot = _asyncNext % MaxDmaBufs;
                const size_t chunk = std::min(remaining, DmaBufferBytes);
                std::memcpy(_dmaBuf[slot], p, chunk);

                spi_transaction_t *t = &_asyncTrans[_asyncNext];
                t->flags = (remaining > chunk && isPinValid(_pinCs)) ? SPI_TRANS_CS_KEEP_ACTIVE : 0;
                t->length = static_cast<int>(chunk * 8U);
                t->rxlength = 0;
                t->tx_buffer = _dmaBuf[slot];
                t->rx_buffer = nullptr;
                t->user = packDcInfo(_pinDc, 1);

                esp_err_t err = spi_device_queue_trans(static_cast<spi_device_handle_t>(_spiHandle), t, portMAX_DELAY);
                if (err != ESP_OK)
                    return fail(st7789::IoError::QueueTransmit);

                _asyncNext = (_asyncNext + 1) % MaxAsyncTrans;
                _asyncInFlight++;

                p += chunk;
                remaining -= chunk;
            }
            return true;
        }
    }

    bool IRAM_ATTR St7789Spi::writePixels(const void *data, size_t len)
    {
        return writePixelsImpl(data, len, true);
    }

    bool IRAM_ATTR St7789Spi::writePixelsAsync(const void *data, size_t len)
    {
        return writePixelsImpl(data, len, false);
    }

    bool IRAM_ATTR St7789Spi::fillPixels(uint16_t color, size_t count)
    {
        if (!_spiHandle || !_dmaBuf[0] || !_dmaBuf[1])
            return fail(st7789::IoError::NotReady);

        if (!acquireBus())
            return false;

        uint16_t *buf0 = reinterpret_cast<uint16_t *>(_dmaBuf[0]);
        uint16_t *buf1 = reinterpret_cast<uint16_t *>(_dmaBuf[1]);
        const size_t bufSizePixels = DmaBufferBytes / sizeof(uint16_t);

        const size_t fillLen = std::min(count, bufSizePixels);
        std::fill_n(buf0, fillLen, color);
        std::fill_n(buf1, fillLen, color);

        size_t remaining = count;
        while (remaining)
        {
            while (_asyncInFlight >= MaxDmaBufs)
            {
                if (!waitOldest())
                    return false;
            }

            const int slot = _asyncNext % MaxDmaBufs;
            const size_t n = std::min(remaining, bufSizePixels);
            spi_transaction_t *t = &_asyncTrans[_asyncNext];
            t->flags = (remaining > n && isPinValid(_pinCs)) ? SPI_TRANS_CS_KEEP_ACTIVE : 0;
            t->length = static_cast<int>(n * sizeof(uint16_t) * 8U);
            t->rxlength = 0;
            t->tx_buffer = _dmaBuf[slot];
            t->rx_buffer = nullptr;
            t->user = packDcInfo(_pinDc, 1);

            const esp_err_t err = spi_device_queue_trans(static_cast<spi_device_handle_t>(_spiHandle), t, portMAX_DELAY);
            if (err != ESP_OK)
            {
                return fail(st7789::IoError::QueueTransmit);
            }

            _asyncNext = (_asyncNext + 1) % MaxAsyncTrans;
            _asyncInFlight++;

            remaining -= n;
        }

        return true;
    }

    bool IRAM_ATTR St7789Spi::waitComplete()
    {
        bool success = drainQueue();
        releaseBus();
        return success;
    }

    bool St7789Spi::flush()
    {
        return waitComplete();
    }
}

#endif
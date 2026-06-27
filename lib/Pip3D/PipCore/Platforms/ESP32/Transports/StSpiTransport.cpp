#include <PipCore/Features.hpp>

#if PIPCORE_TARGET_ESP32

#include <PipCore/Platforms/ESP32/Transports/StSpiTransport.hpp>

#include <sdkconfig.h>
#include <esp_heap_caps.h>
#include <esp_attr.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <hal/gpio_ll.h>
#include <esp_rom_gpio.h>
#include <esp_rom_sys.h>

#if __has_include(<esp_memory_utils.h>)
#include <esp_memory_utils.h>
#endif
#if __has_include(<soc/soc_memory_layout.h>)
#include <soc/soc_memory_layout.h>
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <PipCore/Debug/MemoryHooks.hpp>

#include <cstring>
#include <algorithm>

namespace pipcore::esp32
{
    namespace
    {
        [[nodiscard]] inline constexpr bool isPinValid(int8_t pin) noexcept { return pin >= 0; }
        constexpr size_t DmaBufSize = 16384U;

        static DMA_ATTR uint8_t g_dmaBuf[2][DmaBufSize];

        inline void IRAM_ATTR fastFill32(uint32_t *dest, size_t words, uint32_t value) noexcept
        {
            size_t blocks = words >> 3;
            while (blocks--)
            {
                dest[0] = value;
                dest[1] = value;
                dest[2] = value;
                dest[3] = value;
                dest[4] = value;
                dest[5] = value;
                dest[6] = value;
                dest[7] = value;
                dest += 8;
            }

            size_t remainder = words & 7U;
            while (remainder--)
            {
                *dest++ = value;
            }
        }

        [[nodiscard]] inline bool isDmaCapable(const void *p) noexcept
        {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
            return esp_ptr_internal(p) || esp_ptr_dma_ext_capable(p);
#else
            return esp_ptr_dma_capable(p);
#endif
        }

        inline void IRAM_ATTR gpio_fast_write_high(int8_t pin) noexcept
        {
            if (__builtin_expect(pin < 32, 1))
            {
                GPIO.out_w1ts = (1UL << pin);
            }
            else
            {
                gpio_ll_set_level(&GPIO, static_cast<gpio_num_t>(pin), 1);
            }
        }

        inline void IRAM_ATTR gpio_fast_write_low(int8_t pin) noexcept
        {
            if (__builtin_expect(pin < 32, 1))
            {
                GPIO.out_w1tc = (1UL << pin);
            }
            else
            {
                gpio_ll_set_level(&GPIO, static_cast<gpio_num_t>(pin), 0);
            }
        }

        void IRAM_ATTR lcd_spi_pre_cb(spi_transaction_t *t)
        {
            const uint32_t packed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t->user));
            const int8_t pin = static_cast<int8_t>(packed >> 8);
            const uint8_t level = static_cast<uint8_t>(packed);

            if (pin >= 0)
            {
                if (level)
                {
                    if (__builtin_expect(pin < 32, 1))
                    {
                        GPIO.out_w1ts = (1UL << pin);
                    }
                    else
                    {
                        gpio_ll_set_level(&GPIO, static_cast<gpio_num_t>(pin), 1);
                    }
                }
                else
                {
                    if (__builtin_expect(pin < 32, 1))
                    {
                        GPIO.out_w1tc = (1UL << pin);
                    }
                    else
                    {
                        gpio_ll_set_level(&GPIO, static_cast<gpio_num_t>(pin), 0);
                    }
                }
            }
        }

        [[nodiscard]] inline void *packDcInfo(int8_t pin, uint8_t level) noexcept
        {
            const uint32_t packed = (static_cast<uint32_t>(static_cast<uint8_t>(pin)) << 8) | level;
            return reinterpret_cast<void *>(static_cast<uintptr_t>(packed));
        }

        [[nodiscard]] constexpr int8_t resolveDefaultMosi() noexcept
        {
#if defined(CONFIG_IDF_TARGET_ESP32)
            return 13;
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
            return 11;
#elif defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
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
#elif defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
            return 6;
#else
            return -1;
#endif
        }

        [[nodiscard]] constexpr int8_t resolveDefaultCs() noexcept
        {
#if defined(CONFIG_IDF_TARGET_ESP32)
            return 15;
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || \
    defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) ||   \
    defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
            return 10;
#else
            return -1;
#endif
        }
    }

    void StSpiTransport::configure(int8_t mosi, int8_t sclk, int8_t cs, int8_t dc, int8_t rst, uint32_t hz) noexcept
    {
        deinit();

        _pinMosi = isPinValid(mosi) ? mosi : resolveDefaultMosi();
        _pinSclk = isPinValid(sclk) ? sclk : resolveDefaultSclk();
        _pinCs = isPinValid(cs) ? cs : resolveDefaultCs();

        _pinDc = dc;
        _pinRst = rst;
        _hz = hz ? hz : 80000000U;

        _spiHandle = nullptr;
        _dmaBuf[0] = nullptr;
        _dmaBuf[1] = nullptr;
        _busAcquired = false;
        _initialized = false;
        _lastError = st::IoError::None;

        std::memset(_asyncTrans, 0, sizeof(_asyncTrans));

        _asyncNext = 0;
        _asyncInFlight = 0;

        _lastXs = 0xFFFF;
        _lastXe = 0xFFFF;
        _lastYs = 0xFFFF;
        _lastYe = 0xFFFF;
    }

    StSpiTransport::~StSpiTransport() { StSpiTransport::deinit(); }

    bool StSpiTransport::fail(st::IoError error)
    {
        _lastError = error;
        return false;
    }

    bool StSpiTransport::init()
    {
        clearError();
        if (_initialized)
            return true;

        if (__builtin_expect(!isPinValid(_pinDc), 0))
            return fail(st::IoError::InvalidConfig);

        if (__builtin_expect(!initSpi(), 0))
            return false;

        gpio_config_t io{};
        io.intr_type = GPIO_INTR_DISABLE;
        io.mode = GPIO_MODE_OUTPUT;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.pull_up_en = GPIO_PULLUP_DISABLE;
        io.pin_bit_mask = 1ULL << (uint8_t)_pinDc;

        if (isPinValid(_pinRst))
            io.pin_bit_mask |= 1ULL << (uint8_t)_pinRst;

        if (__builtin_expect(gpio_config(&io) != ESP_OK, 0))
        {
            StSpiTransport::deinit();
            return fail(st::IoError::Gpio);
        }

        _initialized = true;
        return true;
    }

    void StSpiTransport::deinit()
    {
        if (_spiHandle)
        {
            (void)waitComplete();
            spi_bus_remove_device((spi_device_handle_t)_spiHandle);
            spi_bus_free(SPI2_HOST);
            _spiHandle = nullptr;
        }

        _dmaBuf[0] = nullptr;
        _dmaBuf[1] = nullptr;

        _busAcquired = false;
        _initialized = false;
        _asyncNext = 0;
        _asyncInFlight = 0;
    }

    bool StSpiTransport::initSpi()
    {
        if (_spiHandle)
            return true;

        if (__builtin_expect(!isPinValid(_pinMosi) || !isPinValid(_pinSclk), 0))
            return fail(st::IoError::InvalidConfig);

        spi_bus_config_t bus{};
        bus.mosi_io_num = _pinMosi;
        bus.miso_io_num = -1;
        bus.sclk_io_num = _pinSclk;
        bus.quadwp_io_num = -1;
        bus.quadhd_io_num = -1;
        bus.max_transfer_sz = static_cast<int>(HardwareMaxDmaBytes);
#if defined(CONFIG_SPI_MASTER_ISR_IN_IRAM)
        bus.intr_flags = ESP_INTR_FLAG_IRAM;
#endif

        if (__builtin_expect(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK, 0))
            return fail(st::IoError::SpiBusInit);

        spi_device_interface_config_t dev{};
        dev.mode = 0;
        dev.clock_speed_hz = static_cast<int>(_hz);
        dev.spics_io_num = isPinValid(_pinCs) ? _pinCs : -1;
        dev.flags = SPI_DEVICE_NO_DUMMY | SPI_DEVICE_HALFDUPLEX;
        dev.queue_size = MaxAsyncTrans;
        dev.pre_cb = lcd_spi_pre_cb;

        spi_device_handle_t h = nullptr;
        if (__builtin_expect(spi_bus_add_device(SPI2_HOST, &dev, &h) != ESP_OK, 0))
        {
            spi_bus_free(SPI2_HOST);
            return fail(st::IoError::SpiDeviceAdd);
        }
        _spiHandle = h;

        _dmaBuf[0] = g_dmaBuf[0];
        _dmaBuf[1] = g_dmaBuf[1];

        std::memset(_asyncTrans, 0, sizeof(_asyncTrans));

        _asyncTrans[0].user = packDcInfo(_pinDc, 1);
        _asyncTrans[1].user = packDcInfo(_pinDc, 1);

        std::memset(_addrTrans, 0, sizeof(_addrTrans));

        _addrTrans[0].flags = SPI_TRANS_USE_TXDATA;
        _addrTrans[0].length = 8;
        _addrTrans[0].tx_data[0] = 0x2A;
        _addrTrans[0].user = packDcInfo(_pinDc, 0);

        _addrTrans[1].flags = SPI_TRANS_USE_TXDATA;
        _addrTrans[1].length = 32;
        _addrTrans[1].user = packDcInfo(_pinDc, 1);

        _addrTrans[2].flags = SPI_TRANS_USE_TXDATA;
        _addrTrans[2].length = 8;
        _addrTrans[2].tx_data[0] = 0x2B;
        _addrTrans[2].user = packDcInfo(_pinDc, 0);

        _addrTrans[3].flags = SPI_TRANS_USE_TXDATA;
        _addrTrans[3].length = 32;
        _addrTrans[3].user = packDcInfo(_pinDc, 1);

        _addrTrans[4].flags = SPI_TRANS_USE_TXDATA;
        _addrTrans[4].length = 8;
        _addrTrans[4].tx_data[0] = 0x2C;
        _addrTrans[4].user = packDcInfo(_pinDc, 0);

        _busAcquired = false;
        _asyncNext = 0;
        _asyncInFlight = 0;

        _lastXs = 0xFFFF;
        _lastXe = 0xFFFF;
        _lastYs = 0xFFFF;
        _lastYe = 0xFFFF;

        clearError();
        return true;
    }

    bool StSpiTransport::setRst(bool level)
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

    void StSpiTransport::delayMs(uint32_t ms)
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    bool IRAM_ATTR StSpiTransport::writeCommand(uint8_t cmd)
    {
        if (__builtin_expect(!_spiHandle, 0))
            return fail(st::IoError::NotReady);

        if (_asyncInFlight > 0)
        {
            if (__builtin_expect(!drainQueue(), 0))
                return false;
        }

        spi_transaction_t t{};
        t.flags = SPI_TRANS_USE_TXDATA;
        t.length = 8;
        t.user = packDcInfo(_pinDc, 0);
        t.tx_data[0] = cmd;

        if (__builtin_expect(spi_device_polling_transmit(static_cast<spi_device_handle_t>(_spiHandle), &t) != ESP_OK, 0))
            return fail(st::IoError::CommandTransmit);

        return true;
    }

    bool IRAM_ATTR StSpiTransport::write(const void *data, size_t len)
    {
        if (__builtin_expect(!len || !_spiHandle, 0))
            return fail(st::IoError::NotReady);

        if (_asyncInFlight > 0)
        {
            if (__builtin_expect(!drainQueue(), 0))
                return false;
        }

        spi_transaction_t t{};
        t.user = packDcInfo(_pinDc, 1);
        t.length = static_cast<int>(len << 3);

        if (len <= 4U)
        {
            t.flags = SPI_TRANS_USE_TXDATA;
            std::memcpy(t.tx_data, data, len);
        }
        else
        {
            t.tx_buffer = data;
        }

        if (__builtin_expect(spi_device_polling_transmit(static_cast<spi_device_handle_t>(_spiHandle), &t) != ESP_OK, 0))
            return fail(st::IoError::DataTransmit);

        return true;
    }

    bool IRAM_ATTR StSpiTransport::acquireBus()
    {
        _busAcquired = true;
        return true;
    }

    void StSpiTransport::releaseBus()
    {
        _busAcquired = false;
    }

    bool IRAM_ATTR StSpiTransport::waitOldest()
    {
        if (_asyncInFlight <= 0)
            return true;

        spi_transaction_t *r = nullptr;

        esp_err_t err = spi_device_get_trans_result(static_cast<spi_device_handle_t>(_spiHandle), &r, portMAX_DELAY);

        if (__builtin_expect(err != ESP_OK || !r, 0))
        {
            _asyncNext = 0;
            _asyncInFlight = 0;
            return fail(st::IoError::QueueResult);
        }

        _asyncInFlight--;
        return true;
    }

    bool IRAM_ATTR StSpiTransport::drainQueue()
    {
        if (__builtin_expect(!_spiHandle, 0))
            return true;

        bool success = true;

        while (_asyncInFlight > 0)
        {
            spi_transaction_t *r = nullptr;
            esp_err_t err = spi_device_get_trans_result(static_cast<spi_device_handle_t>(_spiHandle), &r, portMAX_DELAY);

            if (__builtin_expect(err != ESP_OK || !r, 0))
            {
                success = false;
            }
            _asyncInFlight--;
        }

        _asyncNext = 0;
        _asyncInFlight = 0;

        return success;
    }

    bool IRAM_ATTR StSpiTransport::writeAddrWindow(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye)
    {
        if (_asyncInFlight > 0)
        {
            if (__builtin_expect(!drainQueue(), 0))
                return false;
        }

        if (__builtin_expect(!acquireBus(), 0))
            return false;

        if (xs == _lastXs && xe == _lastXe && ys == _lastYs && ye == _lastYe)
        {
            spi_device_handle_t handle = static_cast<spi_device_handle_t>(_spiHandle);
            if (__builtin_expect(spi_device_polling_transmit(handle, &_addrTrans[4]) != ESP_OK, 0))
                return fail(st::IoError::CommandTransmit);

            return true;
        }

        _lastXs = xs;
        _lastXe = xe;
        _lastYs = ys;
        _lastYe = ye;

        _addrTrans[1].tx_data[0] = xs >> 8;
        _addrTrans[1].tx_data[1] = xs & 0xFF;
        _addrTrans[1].tx_data[2] = xe >> 8;
        _addrTrans[1].tx_data[3] = xe & 0xFF;

        _addrTrans[3].tx_data[0] = ys >> 8;
        _addrTrans[3].tx_data[1] = ys & 0xFF;
        _addrTrans[3].tx_data[2] = ye >> 8;
        _addrTrans[3].tx_data[3] = ye & 0xFF;

        spi_device_handle_t handle = static_cast<spi_device_handle_t>(_spiHandle);

        if (__builtin_expect(spi_device_polling_transmit(handle, &_addrTrans[0]) != ESP_OK, 0))
            return fail(st::IoError::CommandTransmit);
        if (__builtin_expect(spi_device_polling_transmit(handle, &_addrTrans[1]) != ESP_OK, 0))
            return fail(st::IoError::CommandTransmit);
        if (__builtin_expect(spi_device_polling_transmit(handle, &_addrTrans[2]) != ESP_OK, 0))
            return fail(st::IoError::CommandTransmit);
        if (__builtin_expect(spi_device_polling_transmit(handle, &_addrTrans[3]) != ESP_OK, 0))
            return fail(st::IoError::CommandTransmit);
        if (__builtin_expect(spi_device_polling_transmit(handle, &_addrTrans[4]) != ESP_OK, 0))
            return fail(st::IoError::CommandTransmit);

        return true;
    }

    bool IRAM_ATTR StSpiTransport::writePixelsImpl(const void *data, size_t len, bool useDmaBufferIfNonCapable)
    {
        if (__builtin_expect(!len || !_spiHandle, 0))
            return fail(st::IoError::NotReady);

        if (__builtin_expect(!acquireBus(), 0))
            return false;

        const uint8_t *p = static_cast<const uint8_t *>(data);
        size_t remaining = len;

        const bool directDma = isDmaCapable(p) && ((reinterpret_cast<uintptr_t>(p) & 3U) == 0U);

        if (directDma || !useDmaBufferIfNonCapable)
        {
            if (__builtin_expect(remaining <= HardwareMaxDmaBytes, 1))
            {
                while (_asyncInFlight >= MaxAsyncTrans)
                {
                    if (__builtin_expect(!waitOldest(), 0))
                        return false;
                }

                spi_transaction_t *t = &_asyncTrans[_asyncNext];
                t->flags = 0;
                t->length = static_cast<int>(remaining << 3);
                t->rxlength = 0;
                t->tx_buffer = p;

                esp_err_t err = spi_device_queue_trans(static_cast<spi_device_handle_t>(_spiHandle), t, portMAX_DELAY);
                if (__builtin_expect(err != ESP_OK, 0))
                    return fail(st::IoError::QueueTransmit);

                _asyncNext ^= 1;
                _asyncInFlight++;
                return true;
            }

            while (remaining > 0)
            {
                while (_asyncInFlight >= MaxAsyncTrans)
                {
                    if (__builtin_expect(!waitOldest(), 0))
                        return false;
                }

                const size_t chunk = std::min(remaining, HardwareMaxDmaBytes);
                spi_transaction_t *t = &_asyncTrans[_asyncNext];

                t->flags = 0;
                t->length = static_cast<int>(chunk << 3);
                t->rxlength = 0;
                t->tx_buffer = p;

                esp_err_t err = spi_device_queue_trans(static_cast<spi_device_handle_t>(_spiHandle), t, portMAX_DELAY);
                if (__builtin_expect(err != ESP_OK, 0))
                    return fail(st::IoError::QueueTransmit);

                _asyncNext ^= 1;
                _asyncInFlight++;

                p += chunk;
                remaining -= chunk;
            }
            return true;
        }
        else
        {
            if (__builtin_expect(!_dmaBuf[0] || !_dmaBuf[1], 0))
                return fail(st::IoError::NotReady);

            if (__builtin_expect(remaining <= DmaBufferBytes, 1))
            {
                while (_asyncInFlight >= MaxDmaBufs)
                {
                    if (__builtin_expect(!waitOldest(), 0))
                        return false;
                }

                std::memcpy(_dmaBuf[_asyncNext], p, remaining);

                spi_transaction_t *t = &_asyncTrans[_asyncNext];
                t->flags = 0;
                t->length = static_cast<int>(remaining << 3);
                t->rxlength = 0;
                t->tx_buffer = _dmaBuf[_asyncNext];

                esp_err_t err = spi_device_queue_trans(static_cast<spi_device_handle_t>(_spiHandle), t, portMAX_DELAY);
                if (__builtin_expect(err != ESP_OK, 0))
                    return fail(st::IoError::QueueTransmit);

                _asyncNext ^= 1;
                _asyncInFlight++;
                return true;
            }

            while (remaining > 0)
            {
                while (_asyncInFlight >= MaxDmaBufs)
                {
                    if (__builtin_expect(!waitOldest(), 0))
                        return false;
                }

                const size_t chunk = std::min(remaining, DmaBufferBytes);
                std::memcpy(_dmaBuf[_asyncNext], p, chunk);

                spi_transaction_t *t = &_asyncTrans[_asyncNext];
                t->flags = 0;
                t->length = static_cast<int>(chunk << 3);
                t->rxlength = 0;
                t->tx_buffer = _dmaBuf[_asyncNext];

                esp_err_t err = spi_device_queue_trans(static_cast<spi_device_handle_t>(_spiHandle), t, portMAX_DELAY);
                if (__builtin_expect(err != ESP_OK, 0))
                    return fail(st::IoError::QueueTransmit);

                _asyncNext ^= 1;
                _asyncInFlight++;

                p += chunk;
                remaining -= chunk;
            }
            return true;
        }
    }

    bool IRAM_ATTR StSpiTransport::fillPixels(uint16_t color, size_t count)
    {
        if (__builtin_expect(!_spiHandle || !_dmaBuf[0] || !_dmaBuf[1], 0))
            return fail(st::IoError::NotReady);

        if (__builtin_expect(!acquireBus(), 0))
            return false;

        constexpr size_t bufSizePixels = DmaBufferBytes / sizeof(uint16_t);

        if (__builtin_expect(count <= bufSizePixels, 1))
        {
            uint16_t *buf = reinterpret_cast<uint16_t *>(_dmaBuf[_asyncNext]);
            const uint32_t color32 = (static_cast<uint32_t>(color) << 16) | color;

            fastFill32(reinterpret_cast<uint32_t *>(buf), count >> 1, color32);
            if (count & 1U)
            {
                buf[count - 1] = color;
            }

            while (_asyncInFlight >= MaxDmaBufs)
            {
                if (__builtin_expect(!waitOldest(), 0))
                    return false;
            }

            spi_transaction_t *t = &_asyncTrans[_asyncNext];
            t->flags = 0;
            t->length = static_cast<int>(count << 4);
            t->rxlength = 0;
            t->tx_buffer = _dmaBuf[_asyncNext];

            const esp_err_t err = spi_device_queue_trans(static_cast<spi_device_handle_t>(_spiHandle), t, portMAX_DELAY);
            if (__builtin_expect(err != ESP_OK, 0))
            {
                return fail(st::IoError::QueueTransmit);
            }

            _asyncNext ^= 1;
            _asyncInFlight++;
            return true;
        }

        uint16_t *buf0 = reinterpret_cast<uint16_t *>(_dmaBuf[0]);
        uint16_t *buf1 = reinterpret_cast<uint16_t *>(_dmaBuf[1]);

        const uint32_t color32 = (static_cast<uint32_t>(color) << 16) | color;

        fastFill32(reinterpret_cast<uint32_t *>(buf0), bufSizePixels >> 1, color32);
        fastFill32(reinterpret_cast<uint32_t *>(buf1), bufSizePixels >> 1, color32);

        size_t remaining = count;

        while (remaining)
        {
            while (_asyncInFlight >= MaxDmaBufs)
            {
                if (__builtin_expect(!waitOldest(), 0))
                    return false;
            }

            const size_t n = std::min(remaining, bufSizePixels);
            spi_transaction_t *t = &_asyncTrans[_asyncNext];

            t->flags = 0;
            t->length = static_cast<int>(n << 4);
            t->rxlength = 0;
            t->tx_buffer = _dmaBuf[_asyncNext];

            const esp_err_t err = spi_device_queue_trans(static_cast<spi_device_handle_t>(_spiHandle), t, portMAX_DELAY);
            if (__builtin_expect(err != ESP_OK, 0))
            {
                return fail(st::IoError::QueueTransmit);
            }

            _asyncNext ^= 1;
            _asyncInFlight++;

            remaining -= n;
        }

        return true;
    }

    bool IRAM_ATTR StSpiTransport::waitComplete()
    {
        bool success = drainQueue();
        releaseBus();
        return success;
    }
}

#endif
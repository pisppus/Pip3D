#include <PipCore/Config/Features.hpp>

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789

#include <PipCore/Platforms/ESP32/Transports/St7789Spi.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_rom_gpio.h>
#include <esp_rom_sys.h>
#include <esp_heap_caps.h>
#include <soc/spi_periph.h>
#include <algorithm>

namespace pipcore::esp32
{
    namespace
    {
        constexpr size_t DmaBufferBytes = 8192;

        [[nodiscard]] inline constexpr bool isPinValid(int8_t pin) noexcept { return pin >= 0; }

        [[nodiscard]] inline int8_t getSpi2IomuxMosi() noexcept
        {
#if defined(spi_periph_signal)
            return (int8_t)spi_periph_signal[SPI2_HOST].spid_iomux_pin;
#else
            return -1;
#endif
        }

        [[nodiscard]] inline int8_t getSpi2IomuxSclk() noexcept
        {
#if defined(spi_periph_signal)
            return (int8_t)spi_periph_signal[SPI2_HOST].spiclk_iomux_pin;
#else
            return -1;
#endif
        }

        [[nodiscard]] inline int8_t getSpi2IomuxCs0() noexcept
        {
#if defined(spi_periph_signal)
            return (int8_t)spi_periph_signal[SPI2_HOST].spics0_iomux_pin;
#else
            return -1;
#endif
        }

        [[nodiscard]] int8_t resolveDefaultMosi() noexcept
        {
            int8_t pin = getSpi2IomuxMosi();
            if (isPinValid(pin))
                return pin;
#if defined(CONFIG_IDF_TARGET_ESP32)
            return 13;
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
            return 11;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
            return 11;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
            return 7;
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
            return 7;
#else
            return -1;
#endif
        }

        [[nodiscard]] int8_t resolveDefaultSclk() noexcept
        {
            int8_t pin = getSpi2IomuxSclk();
            if (isPinValid(pin))
                return pin;
#if defined(CONFIG_IDF_TARGET_ESP32)
            return 14;
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
            return 12;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
            return 12;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
            return 6;
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
            return 6;
#else
            return -1;
#endif
        }

        [[nodiscard]] int8_t resolveDefaultCs() noexcept
        {
            int8_t pin = getSpi2IomuxCs0();
            if (isPinValid(pin))
                return pin;
#if defined(CONFIG_IDF_TARGET_ESP32)
            return 15;
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
            return 10;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
            return 10;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
            return 10;
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
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
        _dmaBufSize = DmaBufferBytes;
        _dmaBuf[0] = nullptr;
        _dmaBuf[1] = nullptr;
        _trans[0] = nullptr;
        _trans[1] = nullptr;
        _transInFlight[0] = false;
        _transInFlight[1] = false;
        _dmaNext = 0;
        _dmaInflight = 0;
        _dcLevel = -1;
        _busAcquired = false;
        _initialized = false;
        _lastError = st7789::IoError::None;

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

        _dcLevel = -1;
        _initialized = true;
        return true;
    }

    void St7789Spi::deinit()
    {
        if (_spiHandle)
        {
            (void)flush();
            spi_bus_remove_device((spi_device_handle_t)_spiHandle);
            spi_bus_free(SPI2_HOST);
            _spiHandle = nullptr;
        }

        for (int i = 0; i < 2; ++i)
        {
            if (_dmaBuf[i])
            {
                heap_caps_free(_dmaBuf[i]);
                _dmaBuf[i] = nullptr;
            }
            if (_trans[i])
            {
                heap_caps_free(_trans[i]);
                _trans[i] = nullptr;
            }
            _transInFlight[i] = false;
        }

        _dmaNext = _dmaInflight = 0;
        _dcLevel = -1;
        _busAcquired = false;
        _initialized = false;
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
        bus.max_transfer_sz = (int)DmaBufferBytes;

        if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK)
            return fail(st7789::IoError::SpiBusInit);

        spi_device_interface_config_t dev{};
        dev.mode = 3;
        dev.clock_speed_hz = (int)_hz;
        dev.spics_io_num = isPinValid(_pinCs) ? _pinCs : -1;
        dev.queue_size = 2;
        dev.cs_ena_pretrans = 1;
        dev.cs_ena_posttrans = 1;

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
        }
        _dmaBufSize = DmaBufferBytes;

        if (!_dmaBuf[0] || !_dmaBuf[1])
        {
            deinit();
            return fail(st7789::IoError::DmaBufferAlloc);
        }
        for (int i = 0; i < 2; ++i)
        {
            _trans[i] = static_cast<spi_transaction_t *>(heap_caps_calloc(1, sizeof(spi_transaction_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            if (!_trans[i])
            {
                deinit();
                return fail(st7789::IoError::TransactionAlloc);
            }
            _transInFlight[i] = false;
        }

        _dmaNext = _dmaInflight = 0;
        _busAcquired = false;
        clearError();
        return true;
    }

    inline bool St7789Spi::setDcCached(int level)
    {
        if (_dcLevel != level)
        {
            if (gpio_set_level((gpio_num_t)_pinDc, level) != ESP_OK)
                return fail(st7789::IoError::Gpio);
            _dcLevel = level;
        }
        return true;
    }

    bool St7789Spi::setRst(bool level)
    {
        if (isPinValid(_pinRst))
        {
            if (gpio_set_level((gpio_num_t)_pinRst, level ? 1 : 0) != ESP_OK)
                return fail(st7789::IoError::Gpio);
        }
        return true;
    }

    void St7789Spi::delayMs(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

    bool St7789Spi::writeCommand(uint8_t cmd)
    {
        if (!_spiHandle)
            return fail(st7789::IoError::NotReady);
        if (!flushQueued())
            return false;
        if (!setDcCached(0))
            return false;

        spi_transaction_t t{};
        t.flags = SPI_TRANS_USE_TXDATA;
        t.length = 8;
        t.tx_data[0] = cmd;
        if (spi_device_polling_transmit((spi_device_handle_t)_spiHandle, &t) != ESP_OK)
            return fail(st7789::IoError::CommandTransmit);
        return true;
    }

    bool St7789Spi::write(const void *data, size_t len)
    {
        if (!len || !_spiHandle)
            return fail(st7789::IoError::NotReady);
        if (!flushQueued())
            return false;
        if (!setDcCached(1))
            return false;

        spi_transaction_t t{};
        t.length = (int)(len * 8U);
        t.tx_buffer = data;
        if (spi_device_polling_transmit((spi_device_handle_t)_spiHandle, &t) != ESP_OK)
            return fail(st7789::IoError::DataTransmit);
        return true;
    }

    bool St7789Spi::acquireBus()
    {
        if (!_spiHandle)
            return fail(st7789::IoError::NotReady);
        if (_busAcquired)
            return true;
        if (spi_device_acquire_bus(static_cast<spi_device_handle_t>(_spiHandle), portMAX_DELAY) != ESP_OK)
            return fail(st7789::IoError::QueueTransmit);
        _busAcquired = true;
        return true;
    }

    void St7789Spi::releaseBus()
    {
        if (!_spiHandle || !_busAcquired)
            return;
        spi_device_release_bus(static_cast<spi_device_handle_t>(_spiHandle));
        _busAcquired = false;
    }

    bool St7789Spi::writePixels(const void *data, size_t len)
    {
        if (!len || !_spiHandle || !_dmaBuf[0] || !_dmaBuf[1] || !_trans[0] || !_trans[1])
            return fail(st7789::IoError::NotReady);

        if (!setDcCached(1))
            return false;

        if (!acquireBus())
            return false;

        const uint8_t *p = static_cast<const uint8_t *>(data);
        size_t remaining = len;

        while (remaining)
        {
            while (_transInFlight[_dmaNext])
            {
                if (!waitQueued())
                {
                    releaseBus();
                    return false;
                }
            }

            const int slot = _dmaNext;
            _dmaNext ^= 1;

            const size_t n = std::min(remaining, _dmaBufSize);
            memcpy(_dmaBuf[slot], p, n);

            spi_transaction_t *t = _trans[slot];
            t->flags = remaining > n ? SPI_TRANS_CS_KEEP_ACTIVE : 0;
            t->length = (int)(n * 8U);
            t->rxlength = 0;
            t->tx_buffer = _dmaBuf[slot];
            t->rx_buffer = nullptr;
            t->user = nullptr;

            const esp_err_t err = spi_device_queue_trans((spi_device_handle_t)_spiHandle, t, portMAX_DELAY);
            if (err != ESP_OK)
            {
                _transInFlight[slot] = false;
                (void)flushQueued();
                releaseBus();
                return fail(st7789::IoError::QueueTransmit);
            }
            _transInFlight[slot] = true;
            ++_dmaInflight;

            p += n;
            remaining -= n;
        }

        releaseBus();
        return true;
    }

    bool St7789Spi::flush()
    {
        const bool ok = flushQueued();
        releaseBus();
        return ok;
    }

    bool St7789Spi::waitQueued()
    {
        if (_dmaInflight <= 0 || !_spiHandle)
            return true;

        spi_transaction_t *r = nullptr;
        const esp_err_t err = spi_device_get_trans_result((spi_device_handle_t)_spiHandle, &r, portMAX_DELAY);
        if (err != ESP_OK || !r)
        {
            _transInFlight[0] = false;
            _transInFlight[1] = false;
            _dmaInflight = 0;
            return fail(st7789::IoError::QueueResult);
        }

        if (r == _trans[0])
            _transInFlight[0] = false;
        else if (r == _trans[1])
            _transInFlight[1] = false;
        else
        {
            _transInFlight[0] = false;
            _transInFlight[1] = false;
            _dmaInflight = 0;
            return fail(st7789::IoError::UnexpectedTransaction);
        }

        --_dmaInflight;
        return true;
    }

    bool St7789Spi::flushQueued()
    {
        while (_dmaInflight > 0)
        {
            if (!waitQueued())
                return false;
        }
        return true;
    }
}

#endif

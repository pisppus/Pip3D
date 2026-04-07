#include <PipCore/Config/Features.hpp>

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ILI9488

#include <PipCore/Platforms/ESP32/Transports/Ili9488Spi.hpp>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/spi_periph.h>
#include <algorithm>
#include <string.h>

namespace pipcore::esp32
{
    namespace
    {
        [[nodiscard]] inline constexpr bool isPinValid(int8_t pin) noexcept { return pin >= 0; }

        [[nodiscard]] inline int8_t getSpi2IomuxMosi() noexcept
        {
#if defined(spi_periph_signal)
            return static_cast<int8_t>(spi_periph_signal[SPI2_HOST].spid_iomux_pin);
#else
            return -1;
#endif
        }

        [[nodiscard]] inline int8_t getSpi2IomuxSclk() noexcept
        {
#if defined(spi_periph_signal)
            return static_cast<int8_t>(spi_periph_signal[SPI2_HOST].spiclk_iomux_pin);
#else
            return -1;
#endif
        }

        [[nodiscard]] inline int8_t getSpi2IomuxCs0() noexcept
        {
#if defined(spi_periph_signal)
            return static_cast<int8_t>(spi_periph_signal[SPI2_HOST].spics0_iomux_pin);
#else
            return -1;
#endif
        }
    }

    Ili9488Spi::~Ili9488Spi()
    {
        deinit();
    }

    void Ili9488Spi::configure(int8_t mosi, int8_t sclk, int8_t cs, int8_t dc, int8_t rst, uint32_t hz) noexcept
    {
        deinit();
        _pinMosi = mosi >= 0 ? mosi : getSpi2IomuxMosi();
        _pinSclk = sclk >= 0 ? sclk : getSpi2IomuxSclk();
        _pinCs = cs >= 0 ? cs : getSpi2IomuxCs0();
        _pinDc = dc;
        _pinRst = rst;
        _hz = hz ? hz : 60000000U;
        _effectiveHz = 0U;
        _spiHandle = nullptr;
        _dmaBuf[0] = nullptr;
        _dmaBuf[1] = nullptr;
        _trans[0] = nullptr;
        _trans[1] = nullptr;
        _transInFlight[0] = false;
        _transInFlight[1] = false;
        _dmaNext = 0;
        _dmaInflight = 0;
        _csLevel = -1;
        _dcLevel = -1;
        _busAcquired = false;
        _initialized = false;
        _useDma = false;
        _lastError = ili9488::IoError::None;
    }

    bool Ili9488Spi::fail(ili9488::IoError error)
    {
        _lastError = error;
        return false;
    }

    bool Ili9488Spi::init()
    {
        clearError();
        if (_initialized)
            return true;
        if (!isPinValid(_pinMosi) || !isPinValid(_pinSclk) || !isPinValid(_pinDc))
            return fail(ili9488::IoError::InvalidConfig);
        if (!initSpi())
            return false;

        gpio_config_t io{};
        io.intr_type = GPIO_INTR_DISABLE;
        io.mode = GPIO_MODE_OUTPUT;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.pull_up_en = GPIO_PULLUP_DISABLE;
        io.pin_bit_mask = 1ULL << static_cast<uint8_t>(_pinDc);
        if (isPinValid(_pinCs))
            io.pin_bit_mask |= 1ULL << static_cast<uint8_t>(_pinCs);
        if (isPinValid(_pinRst))
            io.pin_bit_mask |= 1ULL << static_cast<uint8_t>(_pinRst);
        if (gpio_config(&io) != ESP_OK)
        {
            deinit();
            return fail(ili9488::IoError::Gpio);
        }

        _csLevel = -1;
        _dcLevel = -1;
        if (isPinValid(_pinCs) && !setCsCached(1))
        {
            deinit();
            return false;
        }
        _initialized = true;
        return true;
    }

    void Ili9488Spi::deinit()
    {
        if (_spiHandle)
        {
            (void)flush();
            spi_bus_remove_device(static_cast<spi_device_handle_t>(_spiHandle));
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

        _dmaNext = 0;
        _dmaInflight = 0;
        _csLevel = -1;
        _dcLevel = -1;
        _busAcquired = false;
        _initialized = false;
        _useDma = false;
        _effectiveHz = 0U;
    }

    bool Ili9488Spi::initSpi()
    {
        if (_spiHandle)
            return true;
        if (initDmaPath())
            return true;
        deinit();
        return initPollingPath();
    }

    bool Ili9488Spi::initDmaPath()
    {
        spi_bus_config_t bus{};
        bus.mosi_io_num = _pinMosi;
        bus.miso_io_num = -1;
        bus.sclk_io_num = _pinSclk;
        bus.quadwp_io_num = -1;
        bus.quadhd_io_num = -1;
        bus.max_transfer_sz = static_cast<int>(DmaChunkBytes);
        if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK)
            return fail(ili9488::IoError::SpiBusInit);

        spi_device_interface_config_t dev{};
        dev.mode = 0;
        dev.clock_speed_hz = static_cast<int>(_hz);
        dev.spics_io_num = -1;
        dev.queue_size = 2;
        dev.flags = SPI_DEVICE_NO_DUMMY | SPI_DEVICE_HALFDUPLEX;

        spi_device_handle_t h = nullptr;
        if (spi_bus_add_device(SPI2_HOST, &dev, &h) != ESP_OK)
        {
            spi_bus_free(SPI2_HOST);
            return fail(ili9488::IoError::SpiDeviceAdd);
        }
        _spiHandle = h;
        _effectiveHz = static_cast<uint32_t>(dev.clock_speed_hz);

        for (int i = 0; i < 2; ++i)
        {
            _dmaBuf[i] = static_cast<uint8_t *>(heap_caps_aligned_alloc(16, DmaChunkBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
            if (!_dmaBuf[i])
                return fail(ili9488::IoError::DmaBufferAlloc);

            _trans[i] = static_cast<spi_transaction_t *>(heap_caps_calloc(1, sizeof(spi_transaction_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            if (!_trans[i])
                return fail(ili9488::IoError::TransactionAlloc);
        }

        _useDma = true;
        clearError();
        return true;
    }

    bool Ili9488Spi::initPollingPath()
    {
        spi_bus_config_t bus{};
        bus.mosi_io_num = _pinMosi;
        bus.miso_io_num = -1;
        bus.sclk_io_num = _pinSclk;
        bus.quadwp_io_num = -1;
        bus.quadhd_io_num = -1;
        bus.max_transfer_sz = static_cast<int>(PollChunkBytes);
        if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED) != ESP_OK)
            return fail(ili9488::IoError::SpiBusInit);

        spi_device_interface_config_t dev{};
        dev.mode = 0;
        dev.clock_speed_hz = static_cast<int>(_hz);
        dev.spics_io_num = -1;
        dev.queue_size = 1;
        dev.flags = SPI_DEVICE_NO_DUMMY | SPI_DEVICE_HALFDUPLEX;

        spi_device_handle_t h = nullptr;
        if (spi_bus_add_device(SPI2_HOST, &dev, &h) != ESP_OK)
        {
            spi_bus_free(SPI2_HOST);
            return fail(ili9488::IoError::SpiDeviceAdd);
        }
        _spiHandle = h;
        _effectiveHz = static_cast<uint32_t>(dev.clock_speed_hz);
        _useDma = false;
        clearError();
        return true;
    }

    inline bool Ili9488Spi::setCsCached(int level)
    {
        if (!isPinValid(_pinCs))
            return true;
        if (_csLevel != level)
        {
            if (gpio_set_level(static_cast<gpio_num_t>(_pinCs), level) != ESP_OK)
                return fail(ili9488::IoError::Gpio);
            _csLevel = static_cast<int8_t>(level);
        }
        return true;
    }

    inline bool Ili9488Spi::setDcCached(int level)
    {
        if (_dcLevel != level)
        {
            if (gpio_set_level(static_cast<gpio_num_t>(_pinDc), level) != ESP_OK)
                return fail(ili9488::IoError::Gpio);
            _dcLevel = static_cast<int8_t>(level);
        }
        return true;
    }

    bool Ili9488Spi::setRst(bool level)
    {
        if (isPinValid(_pinRst))
        {
            if (gpio_set_level(static_cast<gpio_num_t>(_pinRst), level ? 1 : 0) != ESP_OK)
                return fail(ili9488::IoError::Gpio);
        }
        return true;
    }

    void Ili9488Spi::delayMs(uint32_t ms)
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    bool Ili9488Spi::writeCommand(uint8_t cmd)
    {
        if (!_spiHandle)
            return fail(ili9488::IoError::NotReady);
        if (!flushQueued())
            return false;
        const bool ownBus = !_busAcquired;
        if (ownBus && !acquireBus())
            return false;
        if (!setCsCached(0))
            return false;
        if (!setDcCached(0))
        {
            if (ownBus)
                releaseBus();
            return false;
        }

        spi_transaction_t t{};
        t.flags = SPI_TRANS_USE_TXDATA;
        t.length = 8;
        t.tx_data[0] = cmd;
        if (spi_device_polling_transmit(static_cast<spi_device_handle_t>(_spiHandle), &t) != ESP_OK)
        {
            if (ownBus)
                releaseBus();
            return fail(ili9488::IoError::CommandTransmit);
        }
        if (ownBus)
            releaseBus();
        return true;
    }

    bool Ili9488Spi::write(const void *data, size_t len)
    {
        if (!len || !_spiHandle)
            return fail(ili9488::IoError::NotReady);
        if (!flushQueued())
            return false;
        const bool ownBus = !_busAcquired;
        if (ownBus && !acquireBus())
            return false;
        if (!setCsCached(0))
            return false;
        if (!setDcCached(1))
        {
            if (ownBus)
                releaseBus();
            return false;
        }

        spi_transaction_t t{};
        if (len <= 4U)
        {
            t.flags = SPI_TRANS_USE_TXDATA;
            t.length = static_cast<int>(len * 8U);
            memcpy(t.tx_data, data, len);
        }
        else
        {
            t.length = static_cast<int>(len * 8U);
            t.tx_buffer = data;
        }
        if (spi_device_polling_transmit(static_cast<spi_device_handle_t>(_spiHandle), &t) != ESP_OK)
        {
            if (ownBus)
                releaseBus();
            return fail(ili9488::IoError::DataTransmit);
        }
        if (ownBus)
            releaseBus();
        return true;
    }

    bool Ili9488Spi::acquireBus()
    {
        if (!_spiHandle)
            return fail(ili9488::IoError::NotReady);
        if (_busAcquired)
            return true;
        if (spi_device_acquire_bus(static_cast<spi_device_handle_t>(_spiHandle), portMAX_DELAY) != ESP_OK)
            return fail(ili9488::IoError::QueueTransmit);
        _busAcquired = true;
        return true;
    }

    void Ili9488Spi::releaseBus()
    {
        if (!_spiHandle || !_busAcquired)
            return;
        (void)setCsCached(1);
        spi_device_release_bus(static_cast<spi_device_handle_t>(_spiHandle));
        _busAcquired = false;
    }

    bool Ili9488Spi::writePixels(const void *data, size_t len)
    {
        if (!len || !_spiHandle)
            return fail(ili9488::IoError::NotReady);
        const bool ownBus = !_busAcquired;
        if (!setCsCached(0))
            return false;
        if (!setDcCached(1))
            return false;
        if (ownBus && !acquireBus())
            return false;

        if (!_useDma)
        {
            spi_transaction_t t{};
            t.length = static_cast<int>(len * 8U);
            t.tx_buffer = data;
            if (spi_device_polling_transmit(static_cast<spi_device_handle_t>(_spiHandle), &t) != ESP_OK)
            {
                if (ownBus)
                    releaseBus();
                return fail(ili9488::IoError::DataTransmit);
            }
            if (ownBus)
                releaseBus();
            return true;
        }

        if (!_dmaBuf[0] || !_dmaBuf[1] || !_trans[0] || !_trans[1])
        {
            if (ownBus)
                releaseBus();
            return fail(ili9488::IoError::NotReady);
        }

        const uint8_t *p = static_cast<const uint8_t *>(data);
        size_t remaining = len;

        while (remaining)
        {
            while (_transInFlight[_dmaNext])
            {
                if (!waitQueued())
                {
                    if (ownBus)
                        releaseBus();
                    return false;
                }
            }

            const int slot = _dmaNext;
            _dmaNext ^= 1;
            const size_t n = std::min(remaining, DmaChunkBytes);
            memcpy(_dmaBuf[slot], p, n);

            spi_transaction_t *t = _trans[slot];
            memset(t, 0, sizeof(*t));
            t->flags = (remaining > n) ? SPI_TRANS_CS_KEEP_ACTIVE : 0;
            t->length = static_cast<int>(n * 8U);
            t->tx_buffer = _dmaBuf[slot];

            const esp_err_t err = spi_device_queue_trans(static_cast<spi_device_handle_t>(_spiHandle), t, portMAX_DELAY);
            if (err != ESP_OK)
            {
                _transInFlight[slot] = false;
                (void)flushQueued();
                if (ownBus)
                    releaseBus();
                return fail(ili9488::IoError::QueueTransmit);
            }

            _transInFlight[slot] = true;
            ++_dmaInflight;
            p += n;
            remaining -= n;
        }
        return true;
    }

    uint8_t *Ili9488Spi::directPixelsBuffer(size_t &capacity)
    {
        capacity = 0;
        if (!_useDma || !_spiHandle || !_dmaBuf[0] || !_dmaBuf[1] || !_trans[0] || !_trans[1])
            return nullptr;

        while (_transInFlight[_dmaNext])
        {
            if (!waitQueued())
                return nullptr;
        }

        capacity = DmaChunkBytes;
        return _dmaBuf[_dmaNext];
    }

    bool Ili9488Spi::submitDirectPixels(size_t len)
    {
        if (!len || len > DmaChunkBytes || !_useDma || !_spiHandle)
            return fail(ili9488::IoError::NotReady);
        if (!setCsCached(0))
            return false;
        if (!setDcCached(1))
            return false;
        if (!acquireBus())
            return false;

        while (_transInFlight[_dmaNext])
        {
            if (!waitQueued())
                return false;
        }

        const int slot = _dmaNext;
        _dmaNext ^= 1;

        spi_transaction_t *t = _trans[slot];
        memset(t, 0, sizeof(*t));
        t->flags = SPI_TRANS_CS_KEEP_ACTIVE;
        t->length = static_cast<int>(len * 8U);
        t->tx_buffer = _dmaBuf[slot];

        const esp_err_t err = spi_device_queue_trans(static_cast<spi_device_handle_t>(_spiHandle), t, portMAX_DELAY);
        if (err != ESP_OK)
        {
            _transInFlight[slot] = false;
            return fail(ili9488::IoError::QueueTransmit);
        }

        _transInFlight[slot] = true;
        ++_dmaInflight;
        return true;
    }

    bool Ili9488Spi::flush()
    {
        const bool ok = flushQueued();
        releaseBus();
        return ok;
    }

    bool Ili9488Spi::waitQueued()
    {
        if (_dmaInflight <= 0 || !_spiHandle)
            return true;

        spi_transaction_t *r = nullptr;
        const esp_err_t err = spi_device_get_trans_result(static_cast<spi_device_handle_t>(_spiHandle), &r, portMAX_DELAY);
        if (err != ESP_OK || !r)
        {
            _transInFlight[0] = false;
            _transInFlight[1] = false;
            _dmaInflight = 0;
            return fail(ili9488::IoError::QueueResult);
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
            return fail(ili9488::IoError::UnexpectedTransaction);
        }

        --_dmaInflight;
        return true;
    }

    bool Ili9488Spi::flushQueued()
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

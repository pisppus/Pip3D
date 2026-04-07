#include <PipCore/Platforms/ESP32/Platform.hpp>

namespace pipcore::esp32
{
    void Platform::pinModeInput(uint8_t pin, InputMode mode) noexcept
    {
        _gpio.pinModeInput(pin, mode);
    }

    bool Platform::digitalRead(uint8_t pin) noexcept
    {
        return _gpio.digitalRead(pin);
    }

    void Platform::configureBacklightPin(uint8_t pin, uint8_t channel, uint32_t freqHz, uint8_t resolutionBits) noexcept
    {
        _backlight.configurePin(pin, channel, freqHz, resolutionBits);
    }

    uint32_t Platform::nowMs() noexcept
    {
        return _time.nowMs();
    }

    uint8_t Platform::loadMaxBrightnessPercent() noexcept
    {
#if PIPCORE_ENABLE_PREFS
        uint8_t percent = 100;
        if (!_prefs.loadMaxBrightnessPercent(percent))
        {
            _lastError = PlatformError::PrefsOpenFailed;
            return 100;
        }
        _lastError = PlatformError::None;
        return percent;
#else
        _lastError = PlatformError::None;
        return 100;
#endif
    }

    void Platform::storeMaxBrightnessPercent(uint8_t percent) noexcept
    {
#if PIPCORE_ENABLE_PREFS
        if (!_prefs.storeMaxBrightnessPercent(percent))
        {
            _lastError = PlatformError::PrefsOpenFailed;
            return;
        }
#else
        (void)percent;
#endif
        _lastError = PlatformError::None;
    }

    void Platform::setBacklightPercent(uint8_t percent) noexcept
    {
        _backlight.setPercent(percent);
    }

    void *Platform::alloc(size_t bytes, AllocCaps caps) noexcept
    {
        return _heap.alloc(bytes, caps);
    }

    void Platform::free(void *ptr) noexcept
    {
        _heap.free(ptr);
    }

    bool Platform::configDisplay(const DisplayConfig &cfg) noexcept
    {
        _lastError = PlatformError::None;
        _displayConfigured = false;
        _displayReady = false;
        if (cfg.width == 0 || cfg.height == 0)
        {
            _lastError = PlatformError::InvalidDisplayConfig;
            return false;
        }

        _display.reset();
        _transport.configure(cfg.mosi, cfg.sclk, cfg.cs, cfg.dc, cfg.rst, cfg.hz);

        const bool ok = _display.configure(this, &_transport, cfg.width, cfg.height, cfg.order, cfg.invert, cfg.swap, cfg.xOffset, cfg.yOffset);
        if (!ok)
        {
            _lastError = PlatformError::DisplayConfigureFailed;
            return false;
        }

        _displayConfigured = true;
        return true;
    }

    bool Platform::beginDisplay(uint8_t rotation) noexcept
    {
        _displayReady = false;
        if (!_displayConfigured)
        {
            _lastError = PlatformError::InvalidDisplayConfig;
            return false;
        }

        if (!_display.begin(rotation))
        {
            _lastError = PlatformError::DisplayBeginFailed;
            return false;
        }

        _lastError = PlatformError::None;
        _displayReady = true;
        return true;
    }

    bool Platform::setDisplayRotation(uint8_t rotation) noexcept
    {
        if (!_displayConfigured || !_displayReady)
        {
            _lastError = PlatformError::InvalidDisplayConfig;
            return false;
        }

        if (!_display.setRotation(rotation))
        {
            _lastError = PlatformError::DisplayIoFailed;
            return false;
        }

        _lastError = PlatformError::None;
        return true;
    }

    pipcore::Display *Platform::display() noexcept
    {
        if (!_displayConfigured || !_displayReady)
            return nullptr;
        return &_display;
    }

    uint32_t Platform::freeHeapTotal() noexcept
    {
        return _heap.freeHeapTotal();
    }

    uint32_t Platform::freeHeapInternal() noexcept
    {
        return _heap.freeHeapInternal();
    }

    uint32_t Platform::largestFreeBlock() noexcept
    {
        return _heap.largestFreeBlock();
    }

    uint32_t Platform::minFreeHeap() noexcept
    {
        return _heap.minFreeHeap();
    }

    PlatformError Platform::lastError() const noexcept
    {
        if (_lastError != PlatformError::None)
            return _lastError;

        return _display.ioOk() ? PlatformError::None : PlatformError::DisplayIoFailed;
    }

    const char *Platform::lastErrorText() const noexcept
    {
        if (_lastError == PlatformError::DisplayConfigureFailed ||
            _lastError == PlatformError::DisplayBeginFailed ||
            _lastError == PlatformError::DisplayIoFailed)
        {
            if (!_display.ioOk())
                return _display.lastErrorText();
        }

        if (_lastError != PlatformError::None)
            return platformErrorText(_lastError);

        if (!_display.ioOk())
            return _display.lastErrorText();

        return platformErrorText(PlatformError::None);
    }

    uint8_t Platform::readProgmemByte(const void *addr) noexcept
    {
        return *static_cast<const uint8_t *>(addr);
    }

    pipcore::net::Backend *Platform::network() noexcept
    {
#if PIPCORE_ENABLE_WIFI
        return &_wifi;
#else
        return nullptr;
#endif
    }

    const pipcore::net::Backend *Platform::network() const noexcept
    {
#if PIPCORE_ENABLE_WIFI
        return &_wifi;
#else
        return nullptr;
#endif
    }

    pipcore::ota::Backend *Platform::update() noexcept
    {
#if PIPCORE_ENABLE_OTA
        return &_ota;
#else
        return nullptr;
#endif
    }

    const pipcore::ota::Backend *Platform::update() const noexcept
    {
#if PIPCORE_ENABLE_OTA
        return &_ota;
#else
        return nullptr;
#endif
    }
}

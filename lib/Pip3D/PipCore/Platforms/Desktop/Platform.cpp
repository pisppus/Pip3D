#if defined(_WIN32)

#include <PipCore/Platforms/Desktop/Platform.hpp>
#include <PipCore/Platforms/Desktop/Runtime.hpp>
#include <cstdlib>

namespace pipcore::desktop
{
    uint32_t Platform::nowMs() noexcept
    {
        return Runtime::instance().nowMs();
    }

    void Platform::pinModeInput(uint8_t pin, InputMode mode) noexcept
    {
        Runtime::instance().pinModeInput(pin, mode);
    }

    bool Platform::digitalRead(uint8_t pin) noexcept
    {
        return Runtime::instance().digitalRead(pin);
    }

    uint8_t Platform::loadMaxBrightnessPercent() noexcept
    {
        return _brightness;
    }

    void Platform::storeMaxBrightnessPercent(uint8_t percent) noexcept
    {
        _brightness = (percent > 100U) ? 100U : percent;
    }

    void Platform::setBacklightPercent(uint8_t percent) noexcept
    {
        _brightness = (percent > 100U) ? 100U : percent;
        Runtime::instance().setBacklightPercent(_brightness);
    }

    void *Platform::alloc(size_t bytes, AllocCaps) noexcept
    {
        return std::malloc(bytes);
    }

    void Platform::free(void *ptr) noexcept
    {
        std::free(ptr);
    }

    bool Platform::configDisplay(const DisplayConfig &cfg) noexcept
    {
        if (cfg.width == 0 || cfg.height == 0)
            return false;

        _config = cfg;
        _configValid = _display.configure(cfg.width, cfg.height);
        return _configValid;
    }

    bool Platform::beginDisplay(uint8_t rotation) noexcept
    {
        if (!_configValid)
        {
            DisplayConfig fallback = {};
            fallback.width = 320;
            fallback.height = 240;
            if (!configDisplay(fallback))
                return false;
        }
        return _display.begin(rotation);
    }

    bool Platform::setDisplayRotation(uint8_t rotation) noexcept
    {
        return _display.setRotation(rotation);
    }

    void Platform::WifiBackend::request(bool enabled) noexcept
    {
        _state = enabled ? pipcore::net::WifiState::Unsupported : pipcore::net::WifiState::Off;
    }

    void Platform::OtaBackend::markAppValid() noexcept
    {
        _status.pendingVerify = false;
        notify();
    }

    void Platform::OtaBackend::configure(const pipcore::ota::Options &opt,
                                         pipcore::ota::StatusCallback cb,
                                         void *user) noexcept
    {
        _options = opt;
        _callback = cb;
        _callbackUser = user;
        _status = {};
        _status.state = pipcore::ota::State::Idle;
        _status.lastChangeMs = Runtime::instance().nowMs();
        notify();
    }

    void Platform::OtaBackend::requestCheck(pipcore::ota::CheckMode) noexcept
    {
        fail(pipcore::ota::Error::WifiNotEnabled);
    }

    void Platform::OtaBackend::requestInstall() noexcept
    {
        fail(pipcore::ota::Error::WifiNotEnabled);
    }

    void Platform::OtaBackend::requestStableList() noexcept
    {
        fail(pipcore::ota::Error::WifiNotEnabled);
    }

    const char *Platform::OtaBackend::stableListVersion(uint8_t) const noexcept
    {
        return "";
    }

    void Platform::OtaBackend::requestInstallStableVersion(const char *) noexcept
    {
        fail(pipcore::ota::Error::WifiNotEnabled);
    }

    void Platform::OtaBackend::cancel() noexcept
    {
        _status.state = pipcore::ota::State::Idle;
        _status.error = pipcore::ota::Error::None;
        _status.lastChangeMs = Runtime::instance().nowMs();
        notify();
    }

    void Platform::OtaBackend::fail(pipcore::ota::Error error) noexcept
    {
        _status.state = pipcore::ota::State::Error;
        _status.error = error;
        _status.lastChangeMs = Runtime::instance().nowMs();
        notify();
    }

    void Platform::OtaBackend::notify() noexcept
    {
        if (_callback)
            _callback(_status, _callbackUser);
    }
}

#endif

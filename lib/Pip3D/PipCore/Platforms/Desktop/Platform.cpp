#include <PipCore/Features.hpp>

#if PIPCORE_TARGET_DESKTOP

#include <PipCore/Platforms/Desktop/Platform.hpp>
#include <PipCore/Platforms/Desktop/Runtime.hpp>
#include <cstdlib>

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#endif

namespace pipcore::desktop
{
    namespace
    {
        [[nodiscard]] constexpr uint8_t clampPercent(uint8_t percent) noexcept
        {
            return (percent > 100U) ? 100U : percent;
        }
    }

    uint32_t Platform::nowMs() noexcept
    {
        return Runtime::instance().nowMs();
    }

    uint64_t Platform::nowUs() noexcept
    {
        return Runtime::instance().nowMicros();
    }

    void Platform::pinModeInput(uint8_t pin, InputMode mode) noexcept
    {
        Runtime::instance().pinModeInput(pin, mode);
    }

    bool Platform::digitalRead(uint8_t pin) noexcept
    {
        return Runtime::instance().digitalRead(pin);
    }

    int16_t Platform::analogRead(uint8_t pin) noexcept
    {
        return Runtime::instance().analogRead(pin);
    }

    uint8_t Platform::loadMaxBrightnessPercent() noexcept
    {
        return _brightness;
    }

    void Platform::storeMaxBrightnessPercent(uint8_t percent) noexcept
    {
        _brightness = clampPercent(percent);
    }

    void Platform::setBacklightPercent(uint8_t percent) noexcept
    {
        _brightness = clampPercent(percent);
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

    void *Platform::allocAligned(size_t bytes, size_t align, AllocCaps) noexcept
    {
        if (bytes == 0)
            return nullptr;

#if defined(_MSC_VER) || defined(__MINGW32__)
        return _aligned_malloc(bytes, align);
#else
        void *ptr = nullptr;
        if (align < sizeof(void *))
        {
            align = sizeof(void *);
        }
        if (posix_memalign(&ptr, align, bytes) == 0)
        {
            return ptr;
        }
        return nullptr;
#endif
    }

    void Platform::freeAligned(void *ptr) noexcept
    {
        if (!ptr)
            return;

#if defined(_MSC_VER) || defined(__MINGW32__)
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
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
            fallback.width = static_cast<uint16_t>(PIPGUI_SIM_DEFAULT_WIDTH);
            fallback.height = static_cast<uint16_t>(PIPGUI_SIM_DEFAULT_HEIGHT);
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
            _callback(_status, _cbUser);
    }
}

#endif
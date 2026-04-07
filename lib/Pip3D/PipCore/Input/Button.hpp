#pragma once

#include <PipCore/Platform.hpp>
#include <PipCore/Platforms/Select.hpp>
#include <cstdint>

namespace pipcore
{
    enum PullMode : uint8_t
    {
        Pullup,
        Pulldown
    };

    class Button
    {
    public:
        Button(uint8_t pin, PullMode pull = Pullup)
            : Button(nullptr, pin, pull)
        {
        }

        Button(Platform *platform, uint8_t pin, PullMode pull = Pullup)
            : _platform(platform),
              _pin(pin),
              _pull(pull)
        {
            applyPullDefaults();
        }

        void begin()
        {
            Platform *plat = platform();
            if (plat)
            {
                const InputMode inputMode = (_pull == Pullup)
                                                ? InputMode::Pullup
                                                : InputMode::Pulldown;
                plat->pinModeInput(_pin, inputMode);
            }

            bool raw = readRaw();

            _rawState = raw;
            _stableState = raw;
            _pressedFlag = false;
            _lastRawChangeMs = nowMs();
        }

        void update()
        {
            const bool raw = readRaw();
            const uint32_t now = nowMs();

            if (raw != _rawState)
            {
                _rawState = raw;
                _lastRawChangeMs = now;
            }

            if (_stableState != _rawState && (uint32_t)(now - _lastRawChangeMs) >= debounceMs())
            {
                const bool prev = _stableState;
                _stableState = _rawState;
                if (!prev && _stableState)
                    _pressedFlag = true;
            }
        }

        bool wasPressed()
        {
            if (_pressedFlag)
            {
                _pressedFlag = false;
                return true;
            }
            return false;
        }

        [[nodiscard]] bool isDown() const noexcept { return _stableState; }

        [[nodiscard]] uint8_t pin() const noexcept { return _pin; }
        [[nodiscard]] PullMode pullMode() const noexcept { return _pull; }

    private:
        [[nodiscard]] Platform *platform() const noexcept { return _platform ? _platform : GetPlatform(); }

        [[nodiscard]] uint32_t nowMs() const noexcept
        {
            Platform *plat = platform();
            return plat ? plat->nowMs() : 0;
        }

        [[nodiscard]] static constexpr uint32_t debounceMs() noexcept
        {
            return 12;
        }

        void applyPullDefaults() noexcept
        {
            _activeLow = (_pull == Pullup);
        }

        [[nodiscard]] bool readRaw() const noexcept
        {
            Platform *plat = platform();
            const bool v = plat && plat->digitalRead(_pin);
            return _activeLow ? !v : v;
        }

        Platform *_platform = nullptr;
        uint8_t _pin = 0;
        PullMode _pull = Pullup;
        bool _activeLow = true;
        bool _rawState = false;
        bool _stableState = false;
        bool _pressedFlag = false;
        uint32_t _lastRawChangeMs = 0;
    };
}

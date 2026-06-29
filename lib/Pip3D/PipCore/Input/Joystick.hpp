#pragma once

#include <PipCore/Platform.hpp>
#include <PipCore/Platforms/Select.hpp>
#include <PipCore/Input/Button.hpp>
#include <cmath>

namespace pipcore
{
    struct AnalogAxisConfig
    {
        uint8_t pin;
        int16_t minValue;
        int16_t maxValue;
        float deadZone;
        bool inverted;

        constexpr AnalogAxisConfig(uint8_t p = 0xFF,
                                   int16_t minV = 0,
                                   int16_t maxV = 4095,
                                   float dz = 0.12f,
                                   bool inv = false)
            : pin(p),
              minValue(minV),
              maxValue(maxV),
              deadZone(dz),
              inverted(inv)
        {
        }
    };

    class AnalogAxis
    {
    private:
        Platform *_platform;
        AnalogAxisConfig cfg;
        float filtered;
        bool initialized;
        float rangeInv;
        float deadZone;
        float invDeadSpan;

    public:
        AnalogAxis(Platform *plat = nullptr, const AnalogAxisConfig &c = AnalogAxisConfig())
            : _platform(plat), cfg(c), filtered(0.0f), initialized(false), rangeInv(0.0f), deadZone(c.deadZone), invDeadSpan(1.0f)
        {
        }

        void begin()
        {
            if (cfg.pin == 0xFF)
                return;
            filtered = 0.0f;
            initialized = true;

            if (cfg.maxValue > cfg.minValue)
            {
                rangeInv = 1.0f / (float)(cfg.maxValue - cfg.minValue);
            }
            else
            {
                rangeInv = 0.0f;
            }

            deadZone = cfg.deadZone;
            if (deadZone < 0.0f)
                deadZone = 0.0f;
            if (deadZone > 0.999f)
                deadZone = 0.999f;
            float span = 1.0f - deadZone;
            invDeadSpan = (span > 1e-6f) ? (1.0f / span) : 1.0f;
        }

        float update(float deltaTime)
        {
            if (!initialized)
                return filtered;

            Platform *plat = _platform ? _platform : GetPlatform();
            int raw = plat->analogRead(cfg.pin);

            float v = 0.0f;
            if (rangeInv > 0.0f)
            {
                float n = (float)(raw - cfg.minValue) * rangeInv;
                if (n < 0.0f)
                    n = 0.0f;
                if (n > 1.0f)
                    n = 1.0f;
                v = n * 2.0f - 1.0f;
            }

            if (cfg.inverted)
                v = -v;

            float av = std::abs(v);
            if (av < deadZone)
            {
                v = 0.0f;
            }
            else
            {
                float k = (av - deadZone) * invDeadSpan;
                if (k > 1.0f)
                    k = 1.0f;
                v = (v > 0.0f ? 1.0f : -1.0f) * k;
            }

            float alpha;
            if (deltaTime > 0.0f)
            {
                float cutoff = 16.0f;
                alpha = cutoff * deltaTime;
                if (alpha > 1.0f)
                    alpha = 1.0f;
            }
            else
            {
                alpha = 0.25f;
            }

            filtered += (v - filtered) * alpha;
            return filtered;
        }

        float value() const { return filtered; }
    };

    struct JoystickConfig
    {
        AnalogAxisConfig axisX;
        AnalogAxisConfig axisY;
        uint8_t buttonPin;
        PullMode buttonPull;

        constexpr JoystickConfig() : buttonPin(0xFF), buttonPull(Pullup) {}
        constexpr JoystickConfig(const AnalogAxisConfig &x,
                                 const AnalogAxisConfig &y,
                                 uint8_t bPin = 0xFF,
                                 PullMode bPull = Pullup)
            : axisX(x), axisY(y), buttonPin(bPin), buttonPull(bPull) {}
    };

    class Joystick
    {
    private:
        Platform *_platform;
        AnalogAxis ax;
        AnalogAxis ay;
        Button btn;
        bool hasButton;

    public:
        Joystick(Platform *plat = nullptr, const JoystickConfig &cfg = JoystickConfig())
            : _platform(plat),
              ax(plat, cfg.axisX),
              ay(plat, cfg.axisY),
              btn(plat, cfg.buttonPin, cfg.buttonPull),
              hasButton(cfg.buttonPin != 0xFF)
        {
        }

        ~Joystick() = default;

        void begin()
        {
            ax.begin();
            ay.begin();
            if (hasButton)
            {
                btn.begin();
            }
        }

        void update(float deltaTime)
        {
            ax.update(deltaTime);
            ay.update(deltaTime);
            if (hasButton)
            {
                btn.update();
            }
        }

        float x() const { return ax.value(); }
        float y() const { return ay.value(); }

        bool isPressed() const { return hasButton ? btn.isDown() : false; }
        bool wasPressed() { return hasButton ? btn.wasPressed() : false; } 
    };
}
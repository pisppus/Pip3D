#pragma once

#include <PipCore/Input/Button.hpp>
#include <PipCore/Input/Joystick.hpp>

namespace pip3D
{
    namespace input
    {
        struct ButtonConfig
        {
            uint8_t pin;
            pipcore::PullMode pull;

            constexpr ButtonConfig(uint8_t p = 0xFF, pipcore::PullMode pl = pipcore::Pullup)
                : pin(p), pull(pl) {}
        };

        class Button
        {
        private:
            pipcore::Button impl;

        public:
            Button(const ButtonConfig &cfg = ButtonConfig())
                : impl(cfg.pin, cfg.pull) {}

            void begin() { impl.begin(); }
            void update() { impl.update(); }

            bool isPressed() const { return impl.isDown(); }
            bool wasPressed() { return impl.wasPressed(); }
        };

        using AnalogAxisConfig = pipcore::AnalogAxisConfig;
        using AnalogAxis = pipcore::AnalogAxis;
        using JoystickConfig = pipcore::JoystickConfig;
        using Joystick = pipcore::Joystick;
    }
}
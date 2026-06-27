#pragma once

#include <PipCore/Features.hpp>
#include <cstdint>

namespace pipcore
{
    enum class TouchState : uint8_t
    {
        Released = 0,
        Pressed = 1,
        Held = 2,
        Moved = 3
    };

    struct TouchPoint
    {
        uint16_t x = 0;
        uint16_t y = 0;
        uint16_t pressure = 0;
        uint8_t id = 0;
        TouchState state = TouchState::Released;

        [[nodiscard]] bool active() const noexcept { return state != TouchState::Released; }
    };

    struct TouchConfig
    {
        int8_t sda = -1;
        int8_t scl = -1;
        int8_t intr = -1;
        uint8_t i2cAddr = 0x38;
        uint32_t freqHz = 400000;
        uint16_t width = 0;
        uint16_t height = 0;
        uint8_t rotation = 0;
    };

    class Touch
    {
    public:
        static inline constexpr uint8_t MaxPoints = 2;

        virtual ~Touch() = default;

        [[nodiscard]] virtual bool configure(const TouchConfig &cfg) noexcept = 0;

        [[nodiscard]] virtual bool begin() noexcept = 0;
        virtual void end() noexcept = 0;

        virtual void update() noexcept = 0;

        [[nodiscard]] virtual bool ready() const noexcept = 0;
        [[nodiscard]] virtual uint8_t count() const noexcept = 0;
        [[nodiscard]] virtual TouchPoint point(uint8_t index) const noexcept = 0;
        
        [[nodiscard]] bool touched() const noexcept { return count() > 0; }
    };
}
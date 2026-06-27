#pragma once

#include <PipCore/Displays/StDisplay.hpp>
#include <PipCore/Displays/ST7796/Driver.hpp>

namespace pipcore::st7796
{
    class Display final : public pipcore::detail::StDisplay<Driver>
    {
    public:
        Display() : StDisplay("ST7796") {}

        [[nodiscard]] bool configure(pipcore::Platform *platform,
                                     Transport *transport,
                                     uint16_t width,
                                     uint16_t height,
                                     uint8_t order = 1,
                                     bool invert = true,
                                     bool swap = false,
                                     int16_t xOffset = 0,
                                     int16_t yOffset = 0)
        {
            return StDisplay<Driver>::configureBase(platform, transport, width, height, order, invert, swap, xOffset, yOffset);
        }
    };
}
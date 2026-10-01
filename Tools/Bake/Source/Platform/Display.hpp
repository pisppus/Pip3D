#pragma once

#include <cstdint>
#include <cstddef>

#include "Config.hpp"

namespace pipcore
{
    class Display
    {
    public:
        virtual ~Display() = default;

        [[nodiscard]] virtual bool begin(uint8_t rotation) noexcept = 0;
        [[nodiscard]] virtual bool setRotation(uint8_t rotation) noexcept = 0;
        [[nodiscard]] virtual uint16_t width() const noexcept = 0;
        [[nodiscard]] virtual uint16_t height() const noexcept = 0;

        virtual void fillScreen565(uint16_t color565) noexcept = 0;

        virtual void writeRect565(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *pixels,
                                  int32_t stridePixels) noexcept = 0;

        virtual void writeRect565Async(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *pixels,
                                       int32_t stridePixels) noexcept
        {
            writeRect565(x, y, w, h, pixels, stridePixels);
        }
        virtual void waitDMA() noexcept {}
    };
}

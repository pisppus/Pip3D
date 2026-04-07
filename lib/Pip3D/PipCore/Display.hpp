#pragma once

#include <PipCore/Config/Features.hpp>
#include <cstdint>
#include <cstddef>

namespace pipcore
{
    class Display
    {
    public:
        virtual ~Display() = default;

        [[nodiscard]] virtual bool begin(uint8_t rotation) = 0;
        [[nodiscard]] virtual bool setRotation(uint8_t rotation) = 0;
        [[nodiscard]] virtual uint16_t width() const noexcept = 0;
        [[nodiscard]] virtual uint16_t height() const noexcept = 0;

        virtual void fillScreen565(uint16_t color565) = 0;

        virtual void writeRect565(int16_t x,
                                  int16_t y,
                                  int16_t w,
                                  int16_t h,
                                  const uint16_t *pixels,
                                  int32_t stridePixels) = 0;
    };
}

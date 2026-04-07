#pragma once

#include <PipCore/Display.hpp>

namespace pipcore::simulator
{
    class Display final : public pipcore::Display
    {
    public:
        [[nodiscard]] bool configure(uint16_t width, uint16_t height) noexcept;

        [[nodiscard]] bool begin(uint8_t rotation) override;
        [[nodiscard]] bool setRotation(uint8_t rotation) override;
        [[nodiscard]] uint16_t width() const noexcept override;
        [[nodiscard]] uint16_t height() const noexcept override;

        void fillScreen565(uint16_t color565) override;
        void writeRect565(int16_t x,
                          int16_t y,
                          int16_t w,
                          int16_t h,
                          const uint16_t *pixels,
                          int32_t stridePixels) override;
    };
}

#pragma once

#include <PipCore/Display.hpp>
#include <PipCore/Displays/ST7789/Driver.hpp>

namespace pipcore
{
    class Platform;
}

namespace pipcore::st7789
{
    class Display final : public pipcore::Display
    {
    public:
        Display() = default;
        ~Display() override;

        Display(const Display &) = delete;
        Display &operator=(const Display &) = delete;

        Display(Display &&) = delete;
        Display &operator=(Display &&) = delete;

        [[nodiscard]] bool configure(pipcore::Platform *platform,
                                     Transport *transport,
                                     uint16_t width,
                                     uint16_t height,
                                     uint8_t order = 0,
                                     bool invert = true,
                                     bool swap = false,
                                     int16_t xOffset = 0,
                                     int16_t yOffset = 0)
        {
            _platform = platform;
            return _drv.configure(transport, width, height, order, invert, swap, xOffset, yOffset);
        }

        [[nodiscard]] bool begin(uint8_t rotation) override { return _drv.begin(rotation); }
        [[nodiscard]] bool setRotation(uint8_t rotation) override { return _drv.setRotation(rotation); }
        [[nodiscard]] uint16_t width() const noexcept override { return _drv.width(); }
        [[nodiscard]] uint16_t height() const noexcept override { return _drv.height(); }
        void reset() noexcept { _drv.reset(); }
        [[nodiscard]] IoError lastError() const noexcept { return _drv.lastError(); }
        [[nodiscard]] const char *lastErrorText() const noexcept { return _drv.lastErrorText(); }
        [[nodiscard]] bool ioOk() const noexcept { return _drv.lastError() == IoError::None; }

        void fillScreen565(uint16_t color565) override { (void)_drv.fillScreen565(color565, _drv.swapBytes()); }

        void writeRect565(int16_t x,
                          int16_t y,
                          int16_t w,
                          int16_t h,
                          const uint16_t *pixels,
                          int32_t stridePixels) override;

    private:
        pipcore::Platform *_platform = nullptr;
        Driver _drv;

        uint16_t *_lineBuf = nullptr;
        size_t _lineBufCapPixels = 0;
    };
}

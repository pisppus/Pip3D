#if defined(_WIN32)

#include <PipCore/Displays/Simulator/Display.hpp>
#include <PipCore/Platforms/Desktop/Runtime.hpp>

namespace pipcore::simulator
{
    bool Display::configure(uint16_t width, uint16_t height) noexcept
    {
        return pipcore::desktop::Runtime::instance().configureDisplay(width, height);
    }

    bool Display::begin(uint8_t rotation)
    {
        return pipcore::desktop::Runtime::instance().beginDisplay(rotation);
    }

    bool Display::setRotation(uint8_t rotation)
    {
        return pipcore::desktop::Runtime::instance().setDisplayRotation(rotation);
    }

    uint16_t Display::width() const noexcept
    {
        return pipcore::desktop::Runtime::instance().width();
    }

    uint16_t Display::height() const noexcept
    {
        return pipcore::desktop::Runtime::instance().height();
    }

    void Display::fillScreen565(uint16_t color565)
    {
        pipcore::desktop::Runtime::instance().fillScreen565(color565);
    }

    void Display::writeRect565(int16_t x,
                               int16_t y,
                               int16_t w,
                               int16_t h,
                               const uint16_t *pixels,
                               int32_t stridePixels)
    {
        pipcore::desktop::Runtime::instance().writeRect565(x, y, w, h, pixels, stridePixels);
    }
}

#endif

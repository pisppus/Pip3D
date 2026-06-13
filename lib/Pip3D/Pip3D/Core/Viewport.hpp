#pragma once
#include "Core/Platform.hpp"

namespace pip3D
{
    enum ShadingMode
    {
        SHADING_FLAT = 0,
        SHADING_GOURAUD = 1
    };

#ifndef PIP3D_SCREEN_WIDTH
#define PIP3D_SCREEN_WIDTH 320
#endif

#ifndef PIP3D_SCREEN_HEIGHT
#define PIP3D_SCREEN_HEIGHT 240
#endif

#ifndef PIP3D_SCREEN_BAND_COUNT
#define PIP3D_SCREEN_BAND_COUNT 2
#endif

    static constexpr uint16_t SCREEN_WIDTH = PIP3D_SCREEN_WIDTH;
    static constexpr uint16_t SCREEN_HEIGHT = PIP3D_SCREEN_HEIGHT;

    static constexpr uint16_t SCREEN_BAND_COUNT = PIP3D_SCREEN_BAND_COUNT;
    static constexpr uint16_t SCREEN_BAND_HEIGHT = SCREEN_HEIGHT / SCREEN_BAND_COUNT;

    __attribute__((always_inline)) inline int16_t &currentBandOffsetY()
    {
        static int16_t offsetY = 0;
        return offsetY;
    }

    __attribute__((always_inline)) inline int16_t &currentBandHeight()
    {
        static int16_t h = SCREEN_HEIGHT;
        return h;
    }

    __attribute__((always_inline)) inline uint32_t &currentFrameStamp()
    {
        static uint32_t frameStamp = 0;
        return frameStamp;
    }

    struct alignas(16) Display
    {
        uint16_t width = PIP3D_SCREEN_WIDTH, height = PIP3D_SCREEN_HEIGHT;
        int8_t cs = 10, dc = 9, rst = 8, bl = -1;
        uint32_t spi_freq = 80000000;

        Display() = default;
        Display(uint16_t w, uint16_t h) : width(w), height(h) {}
        Display(uint16_t w, uint16_t h, int8_t cs_, int8_t dc_, int8_t rst_)
            : width(w), height(h), cs(cs_), dc(dc_), rst(rst_) {}
    };

    using DisplayConfig = Display;

    struct alignas(8) Viewport
    {
        int16_t x = 0, y = 0;
        uint16_t width = SCREEN_WIDTH, height = SCREEN_HEIGHT;

        Viewport() = default;
        Viewport(int16_t x_, int16_t y_, uint16_t w_, uint16_t h_) : x(x_), y(y_), width(w_), height(h_) {}
        Viewport(uint16_t w_, uint16_t h_) : width(w_), height(h_) {}

        __attribute__((always_inline)) inline bool contains(int16_t px, int16_t py) const
        {
            return px >= x && px < x + width && py >= y && py < y + height;
        }

        __attribute__((always_inline)) inline float aspect() const { return (float)width / height; }
        __attribute__((always_inline)) inline uint32_t area() const { return width * height; }
    };
}
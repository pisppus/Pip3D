#pragma once

#include "Core/Platform.hpp"
#if !defined(PIP3D_PC)
#include <Arduino.h>
#else
#include <stdint.h>
#include <cstring>
#endif

namespace pip3D
{
    class BitmapFont
    {
    private:
        static constexpr uint8_t FONT_WIDTH = 5;
        static constexpr uint8_t FONT_HEIGHT = 7;
        static constexpr uint8_t CHAR_SPACING = 1;

        static const uint8_t font5x7[95][5];

    public:
        static __attribute__((always_inline)) inline void drawChar(uint16_t *framebuffer, int16_t x, int16_t y,
                                                                   char c, uint16_t color, int16_t screenWidth, int16_t screenHeight)
        {
            const uint8_t uc = static_cast<uint8_t>(c);
            const uint8_t *glyph = (uc >= 32 && uc <= 126) ? font5x7[uc - 32] : font5x7['?' - 32];

            if (x >= screenWidth || y >= screenHeight ||
                x + FONT_WIDTH <= 0 || y + FONT_HEIGHT <= 0)
                return;

            uint8_t startRow = 0;
            uint8_t endRow = FONT_HEIGHT;
            if (y < 0)
            {
                startRow = static_cast<uint8_t>(-y);
                if (startRow >= FONT_HEIGHT)
                    return;
            }
            if (y + FONT_HEIGHT > screenHeight)
            {
                endRow = static_cast<uint8_t>(screenHeight - y);
            }

            uint8_t startCol = 0;
            uint8_t endCol = FONT_WIDTH;
            if (x < 0)
            {
                startCol = static_cast<uint8_t>(-x);
                if (startCol >= FONT_WIDTH)
                    return;
            }
            if (x + FONT_WIDTH > screenWidth)
            {
                endCol = static_cast<uint8_t>(screenWidth - x);
            }

            const size_t rowStride = static_cast<size_t>(screenWidth);
            const int16_t baseX = x;

            for (uint8_t row = startRow; row < endRow; ++row)
            {
                const int16_t py = y + row;
                const size_t rowBase = static_cast<size_t>(py) * rowStride;
                const uint16_t mask = 1u << row;

                for (uint8_t col = startCol; col < endCol; ++col)
                {
                    if (glyph[col] & mask)
                        framebuffer[rowBase + (baseX + col)] = color;
                }
            }
        }

        static __attribute__((always_inline)) inline void drawString(uint16_t *framebuffer, int16_t x, int16_t y,
                                                                     const char *text, uint16_t color,
                                                                     int16_t screenWidth, int16_t screenHeight)
        {
            const char *p = text;
            if (!p)
                return;
            if (x >= screenWidth || y >= screenHeight ||
                y + FONT_HEIGHT <= 0 || x + FONT_WIDTH <= 0)
                return;

            const int16_t stepX = FONT_WIDTH + CHAR_SPACING;
            int16_t cursorX = x;
            while (*p)
            {
                drawChar(framebuffer, cursorX, y, *p, color, screenWidth, screenHeight);
                cursorX += stepX;
                ++p;
            }
        }

        static __attribute__((always_inline)) inline int16_t getStringWidth(const char *text)
        {
            if (!text || !*text)
                return 0;

            const char *p = text;
            int16_t len = 0;
            while (*p++)
                ++len;
            return len * (FONT_WIDTH + CHAR_SPACING) - CHAR_SPACING;
        }

        static constexpr uint8_t getCharWidth() { return FONT_WIDTH + CHAR_SPACING; }
        static constexpr uint8_t getCharHeight() { return FONT_HEIGHT; }
    };
}

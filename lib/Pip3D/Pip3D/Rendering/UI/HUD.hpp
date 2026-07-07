#pragma once

#include "Font.hpp"
#include "Rendering/Display/FrameBuffer.hpp"

namespace pip3D
{
    class HudRenderer
    {
    public:
        static __attribute__((always_inline)) inline void drawText(FrameBuffer &framebuffer,
                                                                   int16_t x, int16_t y,
                                                                   const char *text,
                                                                   uint16_t color)
        {
            uint16_t *fb = framebuffer.getBuffer();
            if (!fb || !text || !*text)
                return;

            const DisplayConfig &cfg = framebuffer.getConfig();
            const int16_t bandTop = g_bandOffsetY;
            const int16_t textBottom = static_cast<int16_t>(y + BitmapFont::getCharHeight());
            if (textBottom <= bandTop || y >= bandTop + cfg.height)
                return;

            BitmapFont::drawString(fb, x, static_cast<int16_t>(y - bandTop), text, color,
                                   cfg.width, cfg.height);
        }

        static __attribute__((always_inline)) inline uint16_t getAdaptiveTextColor(FrameBuffer &framebuffer,
                                                                                   const Viewport &viewport,
                                                                                   int16_t x, int16_t y,
                                                                                   int16_t width = 40,
                                                                                   int16_t height = 8)
        {
            uint16_t *fb = framebuffer.getBuffer();
            if (!fb)
                return 0xFFFF;

            const DisplayConfig &cfg = framebuffer.getConfig();
            if (cfg.width == 0 || cfg.height == 0)
                return 0xFFFF;

            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandBottom = static_cast<int16_t>(bandTop + cfg.height);

            if (width > 40)
                width = 40;
            if (height > 8)
                height = 8;
            if (width <= 0 || height <= 0)
                return 0xFFFF;

            const int16_t vpRight = static_cast<int16_t>(viewport.x + viewport.width);
            const int16_t vpBottom = static_cast<int16_t>(viewport.y + viewport.height);
            const int16_t vpLeft = viewport.x;
            const int16_t vpTop = viewport.y;

            uint32_t totalLuma = 0;
            uint8_t samples = 0;
            const size_t rowStride = static_cast<size_t>(cfg.width);

            for (int16_t dy = 0; dy < height; dy += 2)
            {
                const int16_t sy = static_cast<int16_t>(y + dy);
                if (sy < bandTop || sy >= bandBottom || sy < vpTop || sy >= vpBottom)
                    continue;

                const size_t rowBase = static_cast<size_t>(sy - bandTop) * rowStride;

                for (int16_t dx = 0; dx < width; dx += 8)
                {
                    const int16_t sx = static_cast<int16_t>(x + dx);
                    if (sx < 0 || sx >= cfg.width || sx < vpLeft || sx >= vpRight)
                        continue;

                    const Color pixel(fb[rowBase + sx]);
                    totalLuma += (static_cast<uint32_t>(pixel.r5()) * 2392u) + 
                                 (static_cast<uint32_t>(pixel.g6()) * 2348u) + 
                                 (static_cast<uint32_t>(pixel.b5()) * 912u);
                    ++samples;
                }
            }

            if (samples == 0)
                return 0xFFFF;

            return (totalLuma > (128000u * samples)) ? 0x0000u : 0xFFFFu;
        }

        static __attribute__((always_inline)) inline int16_t getTextWidth(const char *text)
        {
            if (!text || !*text)
                return 0;
            return BitmapFont::getStringWidth(text);
        }
    };
}

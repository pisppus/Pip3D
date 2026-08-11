#pragma once

#include "Core/Platform.hpp"
#include "Rendering/Buffers/FrameBuffer.hpp"

#include "Font.hpp"

namespace pip3D
{

    class HudRenderer
    {
    public:
        static PIP3D_FORCE_INLINE void
        drawText(FrameBuffer &framebuffer,
                 int16_t x, int16_t y,
                 const char *text,
                 uint16_t color) noexcept
        {
            uint16_t *fb = framebuffer.getBuffer();
            if (!fb || !text || !*text)
                return;

            const DisplayConfig &cfg = framebuffer.getConfig();
            const int16_t bandTop = g_bandOffsetY;
            const int16_t textBottom = static_cast<int16_t>(
                y + BitmapFont::getCharHeight());

            if (textBottom <= bandTop || y >= bandTop + cfg.height)
                return;

            BitmapFont::drawString(fb, x,
                                   static_cast<int16_t>(y - bandTop),
                                   text, color, cfg.width, cfg.height);
        }

        static PIP3D_NOINLINE PIP3D_HOT uint16_t
        getAdaptiveColor(FrameBuffer &framebuffer,
                         const Viewport &viewport,
                         int16_t x, int16_t y,
                         int16_t width = 40,
                         int16_t height = 8,
                         uint8_t cacheKey = 0) noexcept
        {
            const uint16_t *fb = framebuffer.getBuffer();
            if (!fb)
                return COLOR_WHITE;

            const DisplayConfig &cfg = framebuffer.getConfig();
            if (cfg.width == 0 || cfg.height == 0)
                return COLOR_WHITE;

            AutoColorState &s = slotState(cacheKey);
            if (s.lastFrame == g_frameStamp)
                return s.currentColor;

            if (width > ADAPTIVE_MAX_W)
                width = ADAPTIVE_MAX_W;
            if (height > ADAPTIVE_MAX_H)
                height = ADAPTIVE_MAX_H;
            if (width <= 0 || height <= 0)
                return s.currentColor;

            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandBottom = static_cast<int16_t>(bandTop + cfg.height);
            const int32_t stride = cfg.width;

            uint32_t totalLuma = 0;
            uint8_t samples = 0;

            for (int16_t dy = 0; dy < height; dy += ADAPTIVE_STEP_Y)
            {
                const int16_t sy = static_cast<int16_t>(y + dy);
                if (sy < bandTop || sy >= bandBottom ||
                    sy < viewport.y || sy >= viewport.y + viewport.height)
                    continue;

                const int32_t rowBase =
                    static_cast<int32_t>(sy - bandTop) * stride;

                for (int16_t dx = 0; dx < width; dx += ADAPTIVE_STEP_X)
                {
                    const int16_t sx = static_cast<int16_t>(x + dx);
                    if (sx < 0 || sx >= cfg.width ||
                        sx < viewport.x ||
                        sx >= viewport.x + viewport.width)
                        continue;

                    const uint16_t px = fb[rowBase + sx];

                    const uint32_t r5 = (px >> 11) & 0x1Fu;
                    const uint32_t g6 = (px >> 5) & 0x3Fu;
                    const uint32_t b5 = px & 0x1Fu;

                    totalLuma += r5 * LUMA_W_R + g6 * LUMA_W_G + b5 * LUMA_W_B;
                    ++samples;
                }
            }

            if (samples == 0)
            {

                s.lastFrame = g_frameStamp;
                return s.currentColor;
            }

            const uint32_t avgLuma = totalLuma / samples;

            const int32_t delta = static_cast<int32_t>(avgLuma) -
                                  static_cast<int32_t>(s.smoothedLuma);
            const uint32_t absDelta = (delta < 0)
                                          ? static_cast<uint32_t>(-delta)
                                          : static_cast<uint32_t>(delta);
            const int32_t shift = (absDelta > EMA_BIG_JUMP_THRESHOLD) ? 1 : 2;
            s.smoothedLuma = static_cast<uint32_t>(
                static_cast<int32_t>(s.smoothedLuma) + (delta >> shift));

            if (s.smoothedLuma >= LUMA_THRESHOLD_HIGH)
                s.currentColor = COLOR_BLACK;
            else if (s.smoothedLuma <= LUMA_THRESHOLD_LOW)
                s.currentColor = COLOR_WHITE;

            s.lastFrame = g_frameStamp;
            return s.currentColor;
        }

        static PIP3D_FORCE_INLINE int16_t
        getTextWidth(const char *text) noexcept
        {
            return BitmapFont::getStringWidth(text);
        }

    private:
        static constexpr uint32_t LUMA_W_R = 2392u;
        static constexpr uint32_t LUMA_W_G = 2348u;
        static constexpr uint32_t LUMA_W_B = 912u;

        static constexpr uint32_t LUMA_THRESHOLD_HIGH = 140000u;
        static constexpr uint32_t LUMA_THRESHOLD_LOW = 110000u;
        static constexpr uint32_t EMA_BIG_JUMP_THRESHOLD = 60000u;

        static constexpr int16_t ADAPTIVE_STEP_X = 8;
        static constexpr int16_t ADAPTIVE_STEP_Y = 2;
        static constexpr int16_t ADAPTIVE_MAX_W = 40;
        static constexpr int16_t ADAPTIVE_MAX_H = 8;

        static constexpr uint16_t COLOR_BLACK = 0x0000u;
        static constexpr uint16_t COLOR_WHITE = 0xFFFFu;

        static constexpr uint8_t AUTO_COLOR_SLOTS = 4;

        struct alignas(4) AutoColorState
        {
            uint32_t smoothedLuma = (LUMA_THRESHOLD_HIGH + LUMA_THRESHOLD_LOW) / 2;
            uint32_t lastFrame = 0xFFFFFFFFu;
            uint16_t currentColor = COLOR_WHITE;
        };

        static AutoColorState &slotState(uint8_t key) noexcept
        {
            static AutoColorState slots[AUTO_COLOR_SLOTS];
            return slots[key & (AUTO_COLOR_SLOTS - 1)];
        }
    };
}

#pragma once
#include "Core/Platform.h"

namespace pip3D
{
    inline constexpr float COLOR_BYTE_MAX_F = 255.0f;
    inline constexpr float INV_COLOR_TEMP_STEP = 0.02f;

    struct alignas(2) Color
    {
        uint16_t rgb565;

        constexpr Color() : rgb565(0) {}
        constexpr Color(uint16_t c) : rgb565(c) {}
        constexpr Color(uint8_t r, uint8_t g, uint8_t b) : rgb565(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)) {}

        static constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b)
        {
            return Color(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
        }

        static constexpr Color fromRGB888(uint8_t r, uint8_t g, uint8_t b) { return rgb(r, g, b); }
        static Color fromTemperature(float k) { return temp(k); }

        static Color hsv(float h, float s, float v)
        {
            h -= floorf(h);
            const float h6 = h * 6.0f;
            const int i = static_cast<int>(h6);
            const float f = h6 - static_cast<float>(i);
            const float p = v * (1.0f - s);
            const float q = v * (1.0f - f * s);
            const float t = v * (1.0f - (1.0f - f) * s);

            switch (i % 6)
            {
            case 0:
                return rgb(static_cast<uint8_t>(v * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(t * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(p * COLOR_BYTE_MAX_F));
            case 1:
                return rgb(static_cast<uint8_t>(q * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(v * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(p * COLOR_BYTE_MAX_F));
            case 2:
                return rgb(static_cast<uint8_t>(p * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(v * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(t * COLOR_BYTE_MAX_F));
            case 3:
                return rgb(static_cast<uint8_t>(p * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(q * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(v * COLOR_BYTE_MAX_F));
            case 4:
                return rgb(static_cast<uint8_t>(t * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(p * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(v * COLOR_BYTE_MAX_F));
            case 5:
                return rgb(static_cast<uint8_t>(v * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(p * COLOR_BYTE_MAX_F),
                           static_cast<uint8_t>(q * COLOR_BYTE_MAX_F));
            default:
                return Color(0);
            }
        }

        static Color temp(float k)
        {
            thread_local float lastK = -1.0f;
            thread_local Color lastColor(0);

            const float step = 50.0f;
            const int bucket = static_cast<int>(k * INV_COLOR_TEMP_STEP + 0.5f);
            const float qk = bucket * step;

            if (likely(qk == lastK))
            {
                return lastColor;
            }

            const float t = qk * 0.01f;
            float r, g, b;

            const bool t_le_66 = (t <= 66.0f);

            if (likely(t_le_66))
            {
                r = 255.0f;
            }
            else
            {
                r = clamp(329.7f * powf(t - 60.0f, -0.133f), 0.0f, 255.0f);
            }

            if (likely(t_le_66))
            {
                g = clamp(99.5f * logf(t) - 161.1f, 0.0f, 255.0f);
            }
            else
            {
                g = clamp(288.1f * powf(t - 60.0f, -0.076f), 0.0f, 255.0f);
            }

            if (unlikely(t >= 66.0f))
            {
                b = 255.0f;
            }
            else if (unlikely(t <= 19.0f))
            {
                b = 0.0f;
            }
            else
            {
                b = clamp(138.5f * logf(t - 10.0f) - 305.0f, 0.0f, 255.0f);
            }

            Color result = rgb(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
            lastK = qk;
            lastColor = result;
            return result;
        }

        static constexpr uint16_t BLACK = 0, WHITE = 0xFFFF, RED = 0xF800, GREEN = 0x07E0, BLUE = 0x001F, CYAN = 0x07FF, MAGENTA = 0xF81F, YELLOW = 0xFFE0;
        static constexpr uint16_t GRAY = 0x8410, DARK_GRAY = 0x4208, LIGHT_GRAY = 0xC618, ORANGE = 0xFD20, PINK = 0xF81F, PURPLE = 0x780F, BROWN = 0xA145, LIME = 0x07E0;

        __attribute__((always_inline)) inline Color blend(const Color &c, uint8_t a) const
        {
            if (unlikely(a == 0))
                return *this;
            if (unlikely(a == 255))
                return c;

            const uint32_t alpha = a >> 3;
            const uint32_t ia = 32u - alpha;
            const uint32_t c1 = rgb565, c2 = c.rgb565;

            const uint32_t rb1 = c1 & 0xF81Fu, rb2 = c2 & 0xF81Fu;
            const uint32_t g1 = c1 & 0x07E0u, g2 = c2 & 0x07E0u;

            const uint32_t rb = ((rb1 * ia + rb2 * alpha) >> 5) & 0xF81Fu;
            const uint32_t g = ((g1 * ia + g2 * alpha) >> 5) & 0x07E0u;

            return Color(rb | g);
        }

        __attribute__((always_inline)) inline Color darken(uint8_t amt) const
        {
            if (unlikely(amt == 0))
                return *this;
            if (unlikely(amt == 255))
                return Color(0);

            const uint32_t scale = (255u - amt) >> 3;
            const uint32_t rb = (((rgb565 & 0xF81Fu) * scale) >> 5) & 0xF81Fu;
            const uint32_t g = (((rgb565 & 0x07E0u) * scale) >> 5) & 0x07E0u;

            return Color(static_cast<uint16_t>(rb | g));
        }

        __attribute__((always_inline)) inline Color lighten(uint8_t amt) const
        {
            if (unlikely(amt == 0))
                return *this;

            const uint32_t r = ((rgb565 >> 11) & 0x1F) + ((31 * amt) >> 8);
            const uint32_t g = ((rgb565 >> 5) & 0x3F) + ((63 * amt) >> 8);
            const uint32_t b = (rgb565 & 0x1F) + ((31 * amt) >> 8);

            const uint32_t r_clamped = r > 31 ? 31 : r;
            const uint32_t g_clamped = g > 63 ? 63 : g;
            const uint32_t b_clamped = b > 31 ? 31 : b;

            return Color((r_clamped << 11) | (g_clamped << 5) | b_clamped);
        }

        __attribute__((always_inline)) inline uint8_t brightness() const
        {
            const uint32_t r = (rgb565 >> 11) & 0x1F;
            const uint32_t g = (rgb565 >> 5) & 0x3F;
            const uint32_t b = rgb565 & 0x1F;
            return (r * 77 + g * 151 + b * 28) >> 8;
        }
    };

    struct Palette
    {
        static Color get(const Color *palette, int size, float t)
        {
            if (size <= 0 || !palette)
            {
                return Color(0);
            }
            if (size == 1)
            {
                return palette[0];
            }

            t = clamp(t, 0.0f, 1.0f);
            const float scaled = t * (size - 1);
            const int i = static_cast<int>(scaled);
            if (i >= size - 1)
                return palette[size - 1];

            const float f = scaled - static_cast<float>(i);
            const uint8_t alpha = static_cast<uint8_t>(f * COLOR_BYTE_MAX_F);
            return palette[i].blend(palette[i + 1], alpha);
        }
    };
}
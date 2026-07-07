#pragma once

#include <cstdint>
#include <cmath>

#if defined(__GNUC__) || defined(__clang__)
#define COLOR_FORCE_INLINE inline __attribute__((always_inline))
#else
#define COLOR_FORCE_INLINE inline
#endif

namespace pip3D
{
    inline constexpr float COLOR_BYTE_MAX_F = 255.0f;

    struct Color
    {
        uint16_t rgb565;

        constexpr Color() : rgb565(0) {}
        constexpr Color(uint16_t c) : rgb565(c) {}
        constexpr Color(uint8_t r, uint8_t g, uint8_t b)
            : rgb565(static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))) {}

        static constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b)
        {
            return Color(static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)));
        }

        static constexpr Color fromRGB888(uint8_t r, uint8_t g, uint8_t b)
        {
            return rgb(r, g, b);
        }

        static COLOR_FORCE_INLINE Color fromFloat(float r, float g, float b)
        {
            int ir = static_cast<int>(r * 31.0f + 0.5f);
            int ig = static_cast<int>(g * 63.0f + 0.5f);
            int ib = static_cast<int>(b * 31.0f + 0.5f);

            if (ig < 16)
                ig &= ~1;

            ir = ir < 0 ? 0 : (ir > 31 ? 31 : ir);
            ig = ig < 0 ? 0 : (ig > 63 ? 63 : ig);
            ib = ib < 0 ? 0 : (ib > 31 ? 31 : ib);

            return Color(static_cast<uint16_t>((ir << 11) | (ig << 5) | ib));
        }

        COLOR_FORCE_INLINE void toFloat(float &r, float &g, float &b) const
        {
            static constexpr float INV_31 = 1.0f / 31.0f;
            static constexpr float INV_63 = 1.0f / 63.0f;
            r = static_cast<float>(r5()) * INV_31;
            g = static_cast<float>(g6()) * INV_63;
            b = static_cast<float>(b5()) * INV_31;
        }

        COLOR_FORCE_INLINE constexpr uint16_t r5() const { return (rgb565 >> 11) & 0x1Fu; }
        COLOR_FORCE_INLINE constexpr uint16_t g6() const { return (rgb565 >> 5) & 0x3Fu; }
        COLOR_FORCE_INLINE constexpr uint16_t b5() const { return rgb565 & 0x1Fu; }

        COLOR_FORCE_INLINE constexpr uint32_t rb() const { return rgb565 & 0xF81Fu; }
        COLOR_FORCE_INLINE constexpr uint32_t g() const { return rgb565 & 0x07E0u; }

        COLOR_FORCE_INLINE constexpr Color dither(uint8_t level) const
        {
            const bool bump = (level >= 8u);
            if (!bump)
                return *this;

            const uint32_t r = r5();
            const uint32_t g = g6();
            const uint32_t b = b5();

            const uint8_t sel = level & 3u;
            uint32_t nr = r, ng = g, nb = b;
            if (sel == 0u)
                nr = (r < 31u) ? r + 1u : 31u;
            else if (sel == 1u)
                nb = (b < 31u) ? b + 1u : 31u;
            else
                ng = (g < 63u) ? g + 1u : 63u;

            return Color(static_cast<uint16_t>((nr << 11) | (ng << 5) | nb));
        }

        COLOR_FORCE_INLINE constexpr Color blend256(const Color &c, uint16_t factor) const
        {
            if (factor == 0)
                return *this;
            if (factor >= 256)
                return c;

            const uint32_t alpha = factor >> 3;
            const uint32_t ia = 32u - alpha;
            const uint32_t c1 = rgb565;
            const uint32_t c2 = c.rgb565;

            const uint32_t rb1 = c1 & 0xF81Fu, rb2 = c2 & 0xF81Fu;
            const uint32_t g1 = c1 & 0x07E0u, g2 = c2 & 0x07E0u;

            const uint32_t rb = ((rb1 * ia + rb2 * alpha) >> 5) & 0xF81Fu;
            const uint32_t g = ((g1 * ia + g2 * alpha) >> 5) & 0x07E0u;

            return Color(static_cast<uint16_t>(rb | g));
        }

        COLOR_FORCE_INLINE constexpr Color blend(const Color &c, uint8_t a) const
        {
            if (a == 0)
                return *this;
            if (a == 255)
                return c;

            const uint16_t factor = static_cast<uint16_t>(a) + (a >> 7);
            return blend256(c, factor);
        }

        COLOR_FORCE_INLINE constexpr Color darken(uint8_t amt) const
        {
            if (amt == 0)
                return *this;
            if (amt == 255)
                return Color(0);

            const uint32_t scale = (255u - amt) >> 3;
            const uint32_t rb = (((rgb565 & 0xF81Fu) * scale) >> 5) & 0xF81Fu;
            const uint32_t g = (((rgb565 & 0x07E0u) * scale) >> 5) & 0x07E0u;

            return Color(static_cast<uint16_t>(rb | g));
        }

        COLOR_FORCE_INLINE constexpr Color lighten(uint8_t amt) const
        {
            if (amt == 0)
                return *this;

            const uint32_t r = r5() + (((31 * amt) + 127) >> 8);
            const uint32_t g = g6() + (((63 * amt) + 127) >> 8);
            const uint32_t b = b5() + (((31 * amt) + 127) >> 8);

            const uint32_t r_clamped = r > 31 ? 31 : r;
            const uint32_t g_clamped = g > 63 ? 63 : g;
            const uint32_t b_clamped = b > 31 ? 31 : b;

            return Color(static_cast<uint16_t>((r_clamped << 11) | (g_clamped << 5) | b_clamped));
        }

        COLOR_FORCE_INLINE constexpr uint8_t brightness() const
        {
            return static_cast<uint8_t>((r5() * 77 + g6() * 151 + b5() * 28) >> 8);
        }

        static COLOR_FORCE_INLINE Color hsv(float h, float s, float v)
        {
            h -= static_cast<float>(static_cast<int>(h));
            if (h < 0.0f)
                h += 1.0f;

            const float h6 = h * 6.0f;
            const int i = static_cast<int>(h6);
            const float f = h6 - static_cast<float>(i);
            const float p = v * (1.0f - s);
            const float q = v * (1.0f - f * s);
            const float t = v * (1.0f - (1.0f - f) * s);

            constexpr float scale = 255.0f;
            switch (i % 6)
            {
            case 0:
                return rgb(static_cast<uint8_t>(v * scale),
                           static_cast<uint8_t>(t * scale),
                           static_cast<uint8_t>(p * scale));
            case 1:
                return rgb(static_cast<uint8_t>(q * scale),
                           static_cast<uint8_t>(v * scale),
                           static_cast<uint8_t>(p * scale));
            case 2:
                return rgb(static_cast<uint8_t>(p * scale),
                           static_cast<uint8_t>(v * scale),
                           static_cast<uint8_t>(t * scale));
            case 3:
                return rgb(static_cast<uint8_t>(p * scale),
                           static_cast<uint8_t>(q * scale),
                           static_cast<uint8_t>(v * scale));
            case 4:
                return rgb(static_cast<uint8_t>(t * scale),
                           static_cast<uint8_t>(p * scale),
                           static_cast<uint8_t>(v * scale));
            case 5:
                return rgb(static_cast<uint8_t>(v * scale),
                           static_cast<uint8_t>(p * scale),
                           static_cast<uint8_t>(q * scale));
            default:
                return Color(0);
            }
        }

        static COLOR_FORCE_INLINE Color fromTemperature(float k)
        {
            const float t = k * 0.01f;
            float r, g, b;

            const bool t_le_66 = (t <= 66.0f);

            if (t_le_66)
            {
                r = 255.0f;
            }
            else
            {
                const float val = t - 60.0f;
                r = 329.7f * powf(val < 1.0f ? 1.0f : val, -0.133f);
                r = r < 0.0f ? 0.0f : (r > 255.0f ? 255.0f : r);
            }

            if (t_le_66)
            {
                const float val = t;
                g = 99.5f * logf(val < 1.0f ? 1.0f : val) - 161.1f;
                g = g < 0.0f ? 0.0f : (g > 255.0f ? 255.0f : g);
            }
            else
            {
                const float val = t - 60.0f;
                g = 288.1f * powf(val < 1.0f ? 1.0f : val, -0.076f);
                g = g < 0.0f ? 0.0f : (g > 255.0f ? 255.0f : g);
            }

            if (t >= 66.0f)
            {
                b = 255.0f;
            }
            else if (t <= 19.0f)
            {
                b = 0.0f;
            }
            else
            {
                const float val = t - 10.0f;
                b = 138.5f * logf(val < 1.0f ? 1.0f : val) - 305.0f;
                b = b < 0.0f ? 0.0f : (b > 255.0f ? 255.0f : b);
            }

            return rgb(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
        }

        static constexpr uint16_t BLACK = 0, WHITE = 0xFFFF, RED = 0xF800, GREEN = 0x07E0, BLUE = 0x001F, CYAN = 0x07FF, MAGENTA = 0xF81F, YELLOW = 0xFFE0;
        static constexpr uint16_t GRAY = 0x8410, DARK_GRAY = 0x4208, LIGHT_GRAY = 0xC618, ORANGE = 0xFD20, PINK = 0xFE19, PURPLE = 0x780F, BROWN = 0xA145, LIME = 0x07E0;
    };

    struct Palette
    {
        static Color get(const Color *palette, int size, float t)
        {
            if (size <= 0 || !palette)
                return Color(0);
            if (size == 1)
                return palette[0];

            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            const float scaled = t * (size - 1);
            const int i = static_cast<int>(scaled);
            if (i >= size - 1)
                return palette[size - 1];

            const float f = scaled - static_cast<float>(i);
            const uint8_t alpha = static_cast<uint8_t>(f * 255.0f);
            return palette[i].blend(palette[i + 1], alpha);
        }
    };
}
#undef COLOR_FORCE_INLINE
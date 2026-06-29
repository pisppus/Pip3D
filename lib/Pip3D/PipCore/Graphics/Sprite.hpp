#pragma once

#include <PipCore/Features.hpp>
#include <cstdint>
#include <cstddef>

namespace pipcore
{
    class Display;
    class Platform;

    struct SpriteClip
    {
        int16_t rx1 = 0, ry1 = 0;
        int16_t cw = 0, ch = 0;
        bool visible = false;
    };

    class Sprite
    {
    public:
        Sprite() = default;
        explicit Sprite(Platform *platform) noexcept
            : _platform(platform)
        {
        }
        ~Sprite();

        Sprite(const Sprite &) = delete;
        Sprite &operator=(const Sprite &) = delete;

        void swap(Sprite &other) noexcept;

        [[nodiscard]] bool createSprite(int16_t w, int16_t h);
        void deleteSprite();

        [[nodiscard]] int16_t width() const noexcept { return _w; }
        [[nodiscard]] int16_t height() const noexcept { return _h; }
        void setPlatform(Platform *platform) noexcept { _platform = platform; }

        [[nodiscard]] void *getBuffer() noexcept { return _buf; }
        [[nodiscard]] const void *getBuffer() const noexcept { return _buf; }

        void fillScreen(uint16_t color565);
        void drawPixel(int16_t x, int16_t y, uint16_t color565);
        void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *pixels565);

        void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color565);

        void setClipRect(int16_t x, int16_t y, int16_t w, int16_t h);
        void getClipRect(int32_t *x, int32_t *y, int32_t *w, int32_t *h) const;

        void pushSprite(Sprite *dst, int16_t x, int16_t y) const;
        void writeToDisplay(Display &display, int16_t x, int16_t y, int16_t w, int16_t h) const;

        [[nodiscard]] static constexpr uint16_t color565(uint8_t r, uint8_t g, uint8_t b) noexcept
        {
            return (uint16_t)((((uint16_t)(r >> 3)) << 11) | (((uint16_t)(g >> 2)) << 5) | ((uint16_t)(b >> 3)));
        }

        [[nodiscard]] static constexpr uint16_t swap16(uint16_t v) noexcept
        {
            return __builtin_bswap16(v);
        }

        [[nodiscard]] static constexpr uint8_t u8clamp(int v) noexcept
        {
            if (v < 0)
                return 0;
            if (v > 255)
                return 255;
            return static_cast<uint8_t>(v);
        }

        [[nodiscard]] static constexpr uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t alpha) noexcept
        {
            if (alpha == 0)
                return bg;
            if (alpha == 255)
                return fg;

            const uint32_t a = alpha;
            const uint32_t inv_a = 256U - a;

            const uint32_t r_bg = (bg >> 11) & 0x1FU;
            const uint32_t g_bg = (bg >> 5) & 0x3FU;
            const uint32_t b_bg = bg & 0x1FU;

            const uint32_t r_fg = (fg >> 11) & 0x1FU;
            const uint32_t g_fg = (fg >> 5) & 0x3FU;
            const uint32_t b_fg = fg & 0x1FU;

            const uint32_t r = (r_fg * a + r_bg * inv_a) >> 8U;
            const uint32_t g = (g_fg * a + g_bg * inv_a) >> 8U;
            const uint32_t b = (b_fg * a + b_bg * inv_a) >> 8U;

            return (uint16_t)((r << 11) | (g << 5) | b);
        }

        [[nodiscard]] inline SpriteClip clipRegion(int16_t x, int16_t y, int16_t w, int16_t h) const noexcept
        {
            SpriteClip res{};
            res.rx1 = x > _clipX ? x : _clipX;
            res.ry1 = y > _clipY ? y : _clipY;
            const int16_t rx2 = (x + w) < (_clipX + _clipW) ? (x + w) : (_clipX + _clipW);
            const int16_t ry2 = (y + h) < (_clipY + _clipH) ? (y + h) : (_clipY + _clipH);
            res.cw = static_cast<int16_t>(rx2 - res.rx1);
            res.ch = static_cast<int16_t>(ry2 - res.ry1);
            res.visible = (res.cw > 0 && res.ch > 0);
            return res;
        }

    private:
        [[nodiscard]] Platform *platform() const noexcept { return _platform; }
        void clipNormalize();
        inline void fillRow(uint16_t *dst, int16_t w, uint16_t v);

        [[nodiscard]] static constexpr int16_t clampi16(int16_t v, int16_t lo, int16_t hi) noexcept
        {
            return (v < lo) ? lo : (v > hi ? hi : v);
        }

    private:
        Platform *_platform = nullptr;
        uint16_t *_buf = nullptr;
        int16_t _w = 0;
        int16_t _h = 0;

        int16_t _clipX = 0;
        int16_t _clipY = 0;
        int16_t _clipW = 0;
        int16_t _clipH = 0;
    };
}
#pragma once

#include <PipCore/Config/Features.hpp>
#include <cstdint>
#include <cstddef>

namespace pipcore
{
    class Display;
    class Platform;

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
            uint32_t a = alpha + (alpha >> 7);

            uint32_t bg_g = bg & 0x07E0;
            uint32_t fg_g = fg & 0x07E0;
            uint32_t g = bg_g + (((fg_g - bg_g) * a) >> 8);
            g &= 0x07E0;

            uint32_t bg_r = bg >> 11;
            uint32_t fg_r = fg >> 11;
            uint32_t r = bg_r + (((fg_r - bg_r) * a) >> 8);

            uint32_t bg_b = bg & 0x1F;
            uint32_t fg_b = fg & 0x1F;
            uint32_t b = bg_b + (((fg_b - bg_b) * a) >> 8);

            return (uint16_t)((r << 11) | g | b);
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

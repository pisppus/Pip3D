#include <PipCore/Graphics/Sprite.hpp>
#include <PipCore/Display.hpp>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace pipcore
{
    namespace
    {
        [[nodiscard]] inline constexpr bool hasRepeatedBytes(uint16_t v) noexcept
        {
            return static_cast<uint8_t>(v >> 8) == static_cast<uint8_t>(v);
        }

        inline void fillSwapped565(uint16_t *dst, size_t pixels, uint16_t v) noexcept
        {
            if (pixels == 0)
                return;

            if (hasRepeatedBytes(v))
            {
                memset(dst, v & 0xFF, pixels * sizeof(uint16_t));
                return;
            }

            if ((reinterpret_cast<uintptr_t>(dst) & 2U) != 0U)
            {
                *dst++ = v;
                --pixels;
            }

            const uint32_t v32 = (static_cast<uint32_t>(v) << 16) | v;
            auto *dst32 = reinterpret_cast<uint32_t *>(dst);
            size_t pairs = pixels >> 1;

            while (pairs >= 4)
            {
                dst32[0] = v32;
                dst32[1] = v32;
                dst32[2] = v32;
                dst32[3] = v32;
                dst32 += 4;
                pairs -= 4;
            }
            while (pairs--)
                *dst32++ = v32;
            if ((pixels & 1U) != 0U)
                *reinterpret_cast<uint16_t *>(dst32) = v;
        }
    }

    void Sprite::fillScreen(uint16_t color565)
    {
        if (!_buf || _w <= 0 || _h <= 0)
            return;

        fillSwapped565(_buf, static_cast<size_t>(_w) * static_cast<size_t>(_h), swap16(color565));
    }

    void Sprite::drawPixel(int16_t x, int16_t y, uint16_t color565)
    {
        if (((uint16_t)(x - _clipX) >= (uint16_t)_clipW) |
            ((uint16_t)(y - _clipY) >= (uint16_t)_clipH))
            return;
        *(_buf + y * _w + x) = __builtin_bswap16(color565);
    }

    inline void Sprite::fillRow(uint16_t *dst, int16_t w, uint16_t v)
    {
        fillSwapped565(dst, static_cast<size_t>(std::max<int16_t>(w, 0)), v);
    }

    void Sprite::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color565)
    {
        if (!_buf)
            return;

        const auto clip = clipRegion(x, y, w, h);
        if (!clip.visible)
            return;

        uint16_t v = swap16(color565);
        uint16_t *ptr = _buf + clip.ry1 * _w + clip.rx1;

        if (clip.cw == _w)
        {
            fillSwapped565(ptr, static_cast<size_t>(clip.cw) * static_cast<size_t>(clip.ch), v);
            return;
        }

        int16_t lines = clip.ch;
        while (lines--)
        {
            fillRow(ptr, clip.cw, v);
            ptr += _w;
        }
    }

    void Sprite::pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *__restrict pixels565)
    {
        if (!_buf || !pixels565 || _clipW <= 0 || _clipH <= 0)
            return;

        const auto clip = clipRegion(x, y, w, h);
        if (!clip.visible)
            return;

        if (clip.cw == w && clip.rx1 == 0 && _w == w)
        {
            pipcore::util::copySwap565(_buf + static_cast<size_t>(clip.ry1) * _w,
                                       pixels565 + static_cast<size_t>(clip.ry1 - y) * w,
                                       static_cast<size_t>(clip.cw) * static_cast<size_t>(clip.ch));
            return;
        }

        const uint16_t *srcLine = pixels565 + (size_t)(clip.ry1 - y) * w + (clip.rx1 - x);
        uint16_t *dstLine = _buf + (size_t)clip.ry1 * _w + clip.rx1;

        int16_t lines = clip.ch;
        while (lines--)
        {
            pipcore::util::copySwap565(dstLine, srcLine, static_cast<size_t>(clip.cw));
            srcLine += w;
            dstLine += _w;
        }
    }
}
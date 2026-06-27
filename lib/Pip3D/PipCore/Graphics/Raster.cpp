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

        inline void copySwap565(uint16_t *dst, const uint16_t *src, size_t pixels) noexcept
        {
            if (pixels == 0)
                return;

            const bool canUse32 = (((reinterpret_cast<uintptr_t>(src) | reinterpret_cast<uintptr_t>(dst)) & 1U) == 0U) &&
                                  ((reinterpret_cast<uintptr_t>(src) & 2U) == (reinterpret_cast<uintptr_t>(dst) & 2U));

            if (canUse32)
            {
                if ((reinterpret_cast<uintptr_t>(src) & 2U) != 0U)
                {
                    *dst++ = Sprite::swap16(*src++);
                    --pixels;
                }

                auto *dst32 = reinterpret_cast<uint32_t *>(dst);
                auto *src32 = reinterpret_cast<const uint32_t *>(src);
                size_t pairs = pixels >> 1;

                while (pairs--)
                {
                    const uint32_t p = __builtin_bswap32(*src32++);
                    *dst32++ = (p >> 16) | (p << 16);
                }

                src = reinterpret_cast<const uint16_t *>(src32);
                dst = reinterpret_cast<uint16_t *>(dst32);
                pixels &= 1U;
            }

            while (pixels--)
                *dst++ = Sprite::swap16(*src++);
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

        int16_t rx1 = x > _clipX ? x : _clipX;
        int16_t ry1 = y > _clipY ? y : _clipY;
        int16_t rx2 = (x + w) < (_clipX + _clipW) ? (x + w) : (_clipX + _clipW);
        int16_t ry2 = (y + h) < (_clipY + _clipH) ? (y + h) : (_clipY + _clipH);

        w = rx2 - rx1;
        h = ry2 - ry1;
        if (w <= 0 || h <= 0)
            return;

        uint16_t v = swap16(color565);
        uint16_t *ptr = _buf + ry1 * _w + rx1;

        if (w == _w)
        {
            fillSwapped565(ptr, static_cast<size_t>(w) * static_cast<size_t>(h), v);
            return;
        }

        while (h--)
        {
            fillRow(ptr, w, v);
            ptr += _w;
        }
    }

    void Sprite::pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *__restrict pixels565)
    {
        if (!_buf || !pixels565 || _clipW <= 0 || _clipH <= 0)
            return;

        int16_t rx1 = x > _clipX ? x : _clipX;
        int16_t ry1 = y > _clipY ? y : _clipY;
        int16_t rx2 = (x + w) < (_clipX + _clipW) ? (x + w) : (_clipX + _clipW);
        int16_t ry2 = (y + h) < (_clipY + _clipH) ? (y + h) : (_clipY + _clipH);

        int16_t cw = rx2 - rx1, ch = ry2 - ry1;
        if (cw <= 0 || ch <= 0)
            return;

        if (cw == w && rx1 == 0 && _w == w)
        {
            copySwap565(_buf + static_cast<size_t>(ry1) * _w,
                        pixels565 + static_cast<size_t>(ry1 - y) * w,
                        static_cast<size_t>(cw) * static_cast<size_t>(ch));
            return;
        }

        const uint16_t *srcLine = pixels565 + (size_t)(ry1 - y) * w + (rx1 - x);
        uint16_t *dstLine = _buf + (size_t)ry1 * _w + rx1;

        while (ch--)
        {
            copySwap565(dstLine, srcLine, static_cast<size_t>(cw));
            srcLine += w;
            dstLine += _w;
        }
    }
}

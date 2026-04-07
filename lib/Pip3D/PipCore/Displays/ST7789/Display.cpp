#include <PipCore/Config/Features.hpp>

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789

#include <PipCore/Displays/ST7789/Display.hpp>
#include <PipCore/Platform.hpp>
#include <algorithm>
#include <cstring>

namespace pipcore::st7789
{
    namespace
    {
        constexpr size_t StageTargetPixels = 4096;

        [[nodiscard]] inline constexpr uint16_t bswap16(uint16_t v) noexcept { return __builtin_bswap16(v); }

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
                    *dst++ = bswap16(*src++);
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
                *dst++ = bswap16(*src++);
        }
    }

    Display::~Display()
    {
        if (_lineBuf && _platform)
        {
            _platform->free(_lineBuf);
            _lineBuf = nullptr;
            _lineBufCapPixels = 0;
        }
    }

    void Display::writeRect565(int16_t x, int16_t y, int16_t w, int16_t h,
                               const uint16_t *pixels, int32_t stridePixels)
    {
        if (!pixels || w <= 0 || h <= 0 || stridePixels < w)
            return;

        const int32_t dispW = _drv.width();
        const int32_t dispH = _drv.height();
        if (dispW <= 0 || dispH <= 0)
            return;

        int32_t x0 = x;
        int32_t y0 = y;
        int32_t x1 = static_cast<int32_t>(x) + w - 1;
        int32_t y1 = static_cast<int32_t>(y) + h - 1;
        if (x1 < 0 || y1 < 0 || x0 >= dispW || y0 >= dispH)
            return;

        x0 = std::max<int32_t>(x0, 0);
        y0 = std::max<int32_t>(y0, 0);
        x1 = std::min<int32_t>(x1, dispW - 1);
        y1 = std::min<int32_t>(y1, dispH - 1);

        const int16_t cW = static_cast<int16_t>(x1 - x0 + 1);
        const int16_t cH = static_cast<int16_t>(y1 - y0 + 1);

        pixels += static_cast<size_t>(y0 - y) * stridePixels + (x0 - x);

        if (!_drv.setAddrWindow(static_cast<uint16_t>(x0), static_cast<uint16_t>(y0), static_cast<uint16_t>(x1), static_cast<uint16_t>(y1)))
            return;

        const bool swap = _drv.swapBytes();
        const size_t totalPixels = static_cast<size_t>(cW) * static_cast<size_t>(cH);

        if (cH == 1 || stridePixels == cW)
        {
            if (!_drv.writePixels565(pixels, totalPixels, swap))
                return;
            return;
        }

        {
            const size_t need = std::min(totalPixels, std::max<size_t>(static_cast<size_t>(cW), StageTargetPixels));
            if (_lineBufCapPixels < need)
            {
                uint16_t *newBuf = _platform ? static_cast<uint16_t *>(_platform->alloc(need * sizeof(uint16_t), AllocCaps::PreferInternal)) : nullptr;
                if (newBuf)
                {
                    if (_lineBuf)
                        _platform->free(_lineBuf);
                    _lineBuf = newBuf;
                    _lineBufCapPixels = need;
                }
            }
        }

        if (_lineBuf && _lineBufCapPixels >= static_cast<size_t>(cW))
        {
            const size_t rowsPerBatch = std::max<size_t>(1, _lineBufCapPixels / static_cast<size_t>(cW));
            int16_t yy = 0;
            while (yy < cH)
            {
                const int16_t batchRows = static_cast<int16_t>(std::min<size_t>(rowsPerBatch, static_cast<size_t>(cH - yy)));
                size_t off = 0;
                for (int16_t rowIdx = 0; rowIdx < batchRows; ++rowIdx)
                {
                    const uint16_t *row = pixels + static_cast<size_t>(yy + rowIdx) * stridePixels;
                    if (!swap)
                        std::memcpy(_lineBuf + off, row, static_cast<size_t>(cW) * sizeof(uint16_t));
                    else
                        copySwap565(_lineBuf + off, row, static_cast<size_t>(cW));
                    off += static_cast<size_t>(cW);
                }

                if (!_drv.writePixels565(_lineBuf, off, false))
                    return;
                yy = static_cast<int16_t>(yy + batchRows);
            }
            return;
        }

        for (int16_t yy = 0; yy < cH; ++yy)
        {
            const uint16_t *row = pixels + static_cast<size_t>(yy) * stridePixels;
            if (!_drv.writePixels565(row, static_cast<size_t>(cW), swap))
                return;
        }
    }
}

#endif

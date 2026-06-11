#include <PipCore/Config/Features.hpp>

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789

#if PIPCORE_TARGET_ESP32
#include <esp_heap_caps.h>
#if __has_include(<esp_memory_utils.h>)
#include <esp_memory_utils.h>
#endif
#if __has_include(<soc/soc_memory_layout.h>)
#include <soc/soc_memory_layout.h>
#endif
#endif

#include <PipCore/Displays/ST7789/Display.hpp>
#include <PipCore/Platform.hpp>
#include <algorithm>
#include <cstring>

namespace pipcore::st7789
{
    namespace
    {
        constexpr size_t StageTargetPixels = 4096;
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

    bool Display::configure(pipcore::Platform *platform,
                            Transport *transport,
                            uint16_t width,
                            uint16_t height,
                            uint8_t order,
                            bool invert,
                            bool swap,
                            int16_t xOffset,
                            int16_t yOffset)
    {
        _platform = platform;

        if (_lineBuf && _platform)
        {
            _platform->free(_lineBuf);
            _lineBuf = nullptr;
            _lineBufCapPixels = 0;
        }

        constexpr size_t fixedCap = StageTargetPixels * 2;
        if (_platform)
        {
            _lineBuf = static_cast<uint16_t *>(_platform->alloc(fixedCap * sizeof(uint16_t), AllocCaps::PreferInternal));
            if (_lineBuf)
            {
                _lineBufCapPixels = fixedCap;
            }
        }

        return _drv.configure(transport, width, height, order, invert, swap, xOffset, yOffset);
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

        if ((cH == 1 || stridePixels == cW) && !swap)
        {
            if (_drv.writePixels565(pixels, totalPixels))
            {
                (void)_drv.waitComplete();
            }
            return;
        }

        if (_lineBuf && _lineBufCapPixels >= static_cast<size_t>(cW) * 2)
        {
            const size_t halfCap = _lineBufCapPixels / 2;
            uint16_t *bufs[2] = {_lineBuf, _lineBuf + halfCap};
            int bufIdx = 0;

            const size_t rowsPerBatch = std::max<size_t>(1, halfCap / static_cast<size_t>(cW));
            int16_t yy = 0;
            bool first = true;

            while (yy < cH)
            {
                const int16_t batchRows = static_cast<int16_t>(std::min<size_t>(rowsPerBatch, static_cast<size_t>(cH - yy)));
                uint16_t *activeBuf = bufs[bufIdx];
                size_t off = 0;

                for (int16_t rowIdx = 0; rowIdx < batchRows; ++rowIdx)
                {
                    const uint16_t *row = pixels + static_cast<size_t>(yy + rowIdx) * stridePixels;
                    if (!swap)
                    {
                        std::memcpy(activeBuf + off, row, static_cast<size_t>(cW) * sizeof(uint16_t));
                    }
                    else
                    {
                        copySwap565(activeBuf + off, row, static_cast<size_t>(cW));
                    }
                    off += static_cast<size_t>(cW);
                }

                if (!first)
                {
                    (void)_drv.waitComplete();
                }
                first = false;

                if (!_drv.writePixels565Async(activeBuf, off))
                {
                    return;
                }

                yy = static_cast<int16_t>(yy + batchRows);
                bufIdx ^= 1;
            }

            (void)_drv.waitComplete();
            return;
        }

        for (int16_t yy = 0; yy < cH; ++yy)
        {
            const uint16_t *row = pixels + static_cast<size_t>(yy) * stridePixels;
            if (!swap)
            {
                if (!_drv.writePixels565(row, static_cast<size_t>(cW)))
                    return;
            }
            else
            {
                for (int16_t xx = 0; xx < cW; ++xx)
                {
                    uint16_t swapped = bswap16(row[xx]);
                    if (!_drv.writePixels565(&swapped, 1))
                        return;
                }
            }
        }
        (void)_drv.waitComplete();
    }

    void Display::writeRect565Async(int16_t x, int16_t y, int16_t w, int16_t h,
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

        if (stridePixels == cW)
        {
            const size_t totalPixels = static_cast<size_t>(cW) * static_cast<size_t>(cH);
            (void)_drv.writePixels565Async(pixels, totalPixels);
        }
        else
        {
            for (int16_t yy = 0; yy < cH; ++yy)
            {
                const uint16_t *row = pixels + static_cast<size_t>(yy) * stridePixels;
                if (!_drv.writePixels565Async(row, static_cast<size_t>(cW)))
                    return;
            }
        }
    }
}

#endif
#pragma once

#include <PipCore/Features.hpp>
#include <PipCore/Display.hpp>
#include <PipCore/Platform.hpp>
#include <PipCore/Displays/ST7789/Driver.hpp>
#include <algorithm>
#include <cstring>

#if PIPCORE_TARGET_ESP32
#include <esp_attr.h>
#include <esp_log.h>
#define PIPCORE_ST_HOT IRAM_ATTR
#else
#include <cstdio>
#define PIPCORE_ST_HOT
#endif

namespace pipcore::detail
{
    using pipcore::st::copySwap565;

    template <typename DriverType>
    class StDisplay : public pipcore::Display
    {
    public:
        static inline constexpr size_t StageTargetPixels = 4096;

        explicit StDisplay(const char *logTag) noexcept
            : _logTag(logTag)
        {
        }

        ~StDisplay() override
        {
            freeLineBuf();
        }

        StDisplay(const StDisplay &) = delete;
        StDisplay &operator=(const StDisplay &) = delete;
        StDisplay(StDisplay &&) = delete;
        StDisplay &operator=(StDisplay &&) = delete;

        [[nodiscard]] bool begin(uint8_t rotation) override { return _drv.begin(rotation); }
        [[nodiscard]] bool setRotation(uint8_t rotation) override { return _drv.setRotation(rotation); }
        [[nodiscard]] uint16_t width() const noexcept override { return _drv.width(); }
        [[nodiscard]] uint16_t height() const noexcept override { return _drv.height(); }
        void reset() noexcept { _drv.reset(); }
        [[nodiscard]] auto lastError() const noexcept { return _drv.lastError(); }
        [[nodiscard]] const char *lastErrorText() const noexcept { return _drv.lastErrorText(); }
        [[nodiscard]] bool ioOk() const noexcept { return _drv.lastError() == DriverType::IoError::None; }

        void fillScreen565(uint16_t color565) override
        {
            (void)_drv.fillScreen565(color565, _drv.swapBytes());
        }

        void waitDMA() override { (void)_drv.waitComplete(); }

        PIPCORE_ST_HOT void writeRect565(int16_t x,
                                         int16_t y,
                                         int16_t w,
                                         int16_t h,
                                         const uint16_t *pixels,
                                         int32_t stridePixels) override;

        PIPCORE_ST_HOT void writeRect565Async(int16_t x,
                                              int16_t y,
                                              int16_t w,
                                              int16_t h,
                                              const uint16_t *pixels,
                                              int32_t stridePixels) override;

    protected:
        void freeLineBuf() noexcept
        {
            if (_lineBuf && _platform)
            {
                _platform->free(_lineBuf);
            }
            _lineBuf = nullptr;
            _lineBufCapPixels = 0;
        }

        [[nodiscard]] bool configureBase(pipcore::Platform *platform,
                                         typename DriverType::Transport *transport,
                                         uint16_t width,
                                         uint16_t height,
                                         uint8_t order,
                                         bool invert,
                                         bool swap,
                                         int16_t xOffset,
                                         int16_t yOffset)
        {
            if (!platform)
            {
                freeLineBuf();
                return false;
            }

            _platform = platform;
            freeLineBuf();

            constexpr size_t fixedCap = StageTargetPixels * 2;
            _lineBuf = static_cast<uint16_t *>(_platform->alloc(fixedCap * sizeof(uint16_t), AllocCaps::PreferInternal));

            if (!_lineBuf)
            {
#if PIPCORE_TARGET_ESP32
                ESP_LOGW(_logTag, "OOM: Failed to allocate line buffer (%d bytes) in internal RAM! Falling back to slow stack-buffered mode.", (int)(fixedCap * sizeof(uint16_t)));
#else
                std::printf("[%s Warning] Failed to allocate line buffer (%d bytes)! Falling back to slow stack-buffered mode.\n", _logTag, (int)(fixedCap * sizeof(uint16_t)));
#endif
                _lineBufCapPixels = 0;
            }
            else
            {
                _lineBufCapPixels = fixedCap;
            }

            const bool success = _drv.configure(transport, width, height, order, invert, swap, xOffset, yOffset);
            if (!success)
            {
                freeLineBuf();
                return false;
            }

            return true;
        }

    protected:
        pipcore::Platform *_platform = nullptr;
        DriverType _drv;
        uint16_t *_lineBuf = nullptr;
        size_t _lineBufCapPixels = 0;
        const char *_logTag = nullptr;
    };

    template <typename DriverType>
    PIPCORE_ST_HOT void StDisplay<DriverType>::writeRect565(int16_t x, int16_t y, int16_t w, int16_t h,
                                                            const uint16_t *pixels, int32_t stridePixels)
    {
        if (!pixels || w <= 0 || h <= 0 || stridePixels < w)
            return;

        const int32_t dispW = _drv.width();
        const int32_t dispH = _drv.height();
        if (dispW <= 0 || dispH <= 0)
            return;

        int16_t x0, y0, x1, y1;
        int16_t cW, cH;

        if (x >= 0 && y >= 0 && (x + w) <= dispW && (y + h) <= dispH)
        {
            x0 = x;
            y0 = y;
            x1 = x + w - 1;
            y1 = y + h - 1;
            cW = w;
            cH = h;
        }
        else
        {
            x0 = x;
            y0 = y;
            const int32_t tx1 = static_cast<int32_t>(x) + w - 1;
            const int32_t ty1 = static_cast<int32_t>(y) + h - 1;
            if (tx1 < 0 || ty1 < 0 || x0 >= dispW || y0 >= dispH)
                return;

            x0 = std::max<int16_t>(x0, 0);
            y0 = std::max<int16_t>(y0, 0);
            x1 = static_cast<int16_t>(std::min<int32_t>(tx1, dispW - 1));
            y1 = static_cast<int16_t>(std::min<int32_t>(ty1, dispH - 1));

            cW = static_cast<int16_t>(x1 - x0 + 1);
            cH = static_cast<int16_t>(y1 - y0 + 1);

            pixels += static_cast<size_t>(y0 - y) * stridePixels + (x0 - x);
        }

        if (!_drv.setAddrWindow(static_cast<uint16_t>(x0), static_cast<uint16_t>(y0), static_cast<uint16_t>(x1), static_cast<uint16_t>(y1)))
            return;

        const bool swap = _drv.swapBytes();
        const size_t totalPixels = static_cast<size_t>(cW) * static_cast<size_t>(cH);

        uint16_t *const localLineBuf = _lineBuf;
        const size_t localLineBufCap = _lineBufCapPixels;

        if ((cH == 1 || stridePixels == cW) && !swap)
        {
            if (_drv.writePixels565(pixels, totalPixels))
            {
                (void)_drv.waitComplete();
            }
            return;
        }

        if (stridePixels == cW && swap && localLineBuf)
        {
            const size_t halfCap = localLineBufCap >> 1;
            uint16_t *bufs[2] = {localLineBuf, localLineBuf + halfCap};
            int bufIdx = 0;

            size_t remaining = totalPixels;
            const uint16_t *srcPtr = pixels;
            bool activeTrans[2] = {false, false};

            while (remaining > 0)
            {
                const size_t chunk = std::min(remaining, halfCap);

                if (activeTrans[bufIdx])
                {
                    (void)_drv.waitOldest();
                    activeTrans[bufIdx] = false;
                }

                copySwap565(bufs[bufIdx], srcPtr, chunk);
                if (!_drv.writePixels565Async(bufs[bufIdx], chunk))
                    return;

                activeTrans[bufIdx] = true;
                srcPtr += chunk;
                remaining -= chunk;
                bufIdx ^= 1;
            }
            (void)_drv.waitComplete();
            return;
        }

        if (localLineBuf && localLineBufCap >= static_cast<size_t>(cW) * 2)
        {
            const size_t halfCap = localLineBufCap >> 1;
            uint16_t *bufs[2] = {localLineBuf, localLineBuf + halfCap};
            int bufIdx = 0;

            const size_t rowsPerBatch = std::max<size_t>(1, halfCap / static_cast<size_t>(cW));
            int16_t yy = 0;
            bool activeTrans[2] = {false, false};

            while (yy < cH)
            {
                const int16_t batchRows = static_cast<int16_t>(std::min<size_t>(rowsPerBatch, static_cast<size_t>(cH - yy)));

                if (activeTrans[bufIdx])
                {
                    (void)_drv.waitOldest();
                    activeTrans[bufIdx] = false;
                }

                uint16_t *activeBuf = bufs[bufIdx];
                size_t off = 0;

                const uint16_t *row = pixels + static_cast<size_t>(yy) * stridePixels;

                if (!swap)
                {
#pragma GCC unroll 4
                    for (int16_t rowIdx = 0; rowIdx < batchRows; ++rowIdx)
                    {
                        std::memcpy(activeBuf + off, row, static_cast<size_t>(cW) * sizeof(uint16_t));
                        off += static_cast<size_t>(cW);
                        row += stridePixels;
                    }
                }
                else
                {
#pragma GCC unroll 4
                    for (int16_t rowIdx = 0; rowIdx < batchRows; ++rowIdx)
                    {
                        copySwap565(activeBuf + off, row, static_cast<size_t>(cW));
                        off += static_cast<size_t>(cW);
                        row += stridePixels;
                    }
                }

                if (!_drv.writePixels565Async(activeBuf, off))
                {
                    return;
                }
                activeTrans[bufIdx] = true;

                yy = static_cast<int16_t>(yy + batchRows);
                bufIdx ^= 1;
            }
            (void)_drv.waitComplete();
            return;
        }

        constexpr size_t StackBufPixels = 128;
        uint16_t temp[StackBufPixels];

        if (!swap)
        {
            const uint16_t *row = pixels;
#pragma GCC unroll 4
            for (int16_t yy = 0; yy < cH; ++yy)
            {
                if (!_drv.writePixels565(row, static_cast<size_t>(cW)))
                    return;
                row += stridePixels;
            }
        }
        else
        {
            const uint16_t *row = pixels;
            for (int16_t yy = 0; yy < cH; ++yy)
            {
                size_t remaining = static_cast<size_t>(cW);
                const uint16_t *srcPtr = row;
                while (remaining > 0)
                {
                    const size_t chunk = std::min<size_t>(remaining, StackBufPixels);
                    copySwap565(temp, srcPtr, chunk);
                    if (!_drv.writePixels565(temp, chunk))
                        return;
                    srcPtr += chunk;
                    remaining -= chunk;
                }
                row += stridePixels;
            }
        }
        (void)_drv.waitComplete();
    }

    template <typename DriverType>
    PIPCORE_ST_HOT void StDisplay<DriverType>::writeRect565Async(int16_t x, int16_t y, int16_t w, int16_t h,
                                                                 const uint16_t *pixels, int32_t stridePixels)
    {
        if (!pixels || w <= 0 || h <= 0 || stridePixels < w)
            return;

        const int32_t dispW = _drv.width();
        const int32_t dispH = _drv.height();
        if (dispW <= 0 || dispH <= 0)
            return;

        int16_t x0, y0, x1, y1;
        int16_t cW, cH;

        if (x >= 0 && y >= 0 && (x + w) <= dispW && (y + h) <= dispH)
        {
            x0 = x;
            y0 = y;
            x1 = x + w - 1;
            y1 = y + h - 1;
            cW = w;
            cH = h;
        }
        else
        {
            x0 = x;
            y0 = y;
            const int32_t tx1 = static_cast<int32_t>(x) + w - 1;
            const int32_t ty1 = static_cast<int32_t>(y) + h - 1;
            if (tx1 < 0 || ty1 < 0 || x0 >= dispW || y0 >= dispH)
                return;

            x0 = std::max<int16_t>(x0, 0);
            y0 = std::max<int16_t>(y0, 0);
            x1 = static_cast<int16_t>(std::min<int32_t>(tx1, dispW - 1));
            y1 = static_cast<int16_t>(std::min<int32_t>(ty1, dispH - 1));

            cW = static_cast<int16_t>(x1 - x0 + 1);
            cH = static_cast<int16_t>(y1 - y0 + 1);

            pixels += static_cast<size_t>(y0 - y) * stridePixels + (x0 - x);
        }

        if (!_drv.setAddrWindow(static_cast<uint16_t>(x0), static_cast<uint16_t>(y0), static_cast<uint16_t>(x1), static_cast<uint16_t>(y1)))
            return;

        if (stridePixels == cW)
        {
            const size_t totalPixels = static_cast<size_t>(cW) * static_cast<size_t>(cH);
            (void)_drv.writePixels565Async(pixels, totalPixels);
        }
        else
        {
            const uint16_t *row = pixels;
#pragma GCC unroll 4
            for (int16_t yy = 0; yy < cH; ++yy)
            {
                if (!_drv.writePixels565Async(row, static_cast<size_t>(cW)))
                    return;
                row += stridePixels;
            }
        }
    }
}
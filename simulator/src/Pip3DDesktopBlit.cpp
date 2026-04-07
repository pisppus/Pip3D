#include <Pip3D/Rendering/Display/PcDisplayBlit.h>
#include <PipCore/Platforms/Desktop/Runtime.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pip3D
{
    namespace
    {
        PcBlitCallback g_pcBlitCallback = nullptr;
        uint16_t g_pcWidth = 0;
        uint16_t g_pcHeight = 0;
        bool g_runtimeReady = false;
        std::vector<uint16_t> g_swapBuffer;

        [[nodiscard]] constexpr uint16_t swap565(uint16_t value) noexcept
        {
            return static_cast<uint16_t>((value << 8) | (value >> 8));
        }
    }

    void setPcBlitCallback(PcBlitCallback cb)
    {
        g_pcBlitCallback = cb;
    }

    PcBlitCallback getPcBlitCallback()
    {
        return g_pcBlitCallback;
    }

    bool initPcDisplay(const DisplayConfig &config)
    {
        g_pcWidth = config.width;
        g_pcHeight = config.height;

        if (g_pcBlitCallback)
        {
            g_runtimeReady = false;
            return true;
        }

        auto &runtime = pipcore::desktop::Runtime::instance();
        g_runtimeReady = runtime.configureDisplay(config.width, config.height) &&
                         runtime.beginDisplay(0);
        return g_runtimeReady;
    }

    void blitPcDisplay(int16_t x,
                       int16_t y,
                       int16_t w,
                       int16_t h,
                       const uint16_t *src,
                       int32_t stridePixels)
    {
        if (!src || w <= 0 || h <= 0 || stridePixels < w)
            return;

        if (g_pcBlitCallback)
        {
            g_pcBlitCallback(x, y, w, h, src, stridePixels);
            return;
        }

        if (!g_runtimeReady)
            return;

        const size_t pixelCount = static_cast<size_t>(w) * static_cast<size_t>(h);
        if (g_swapBuffer.size() < pixelCount)
            g_swapBuffer.resize(pixelCount);

        for (int16_t row = 0; row < h; ++row)
        {
            const uint16_t *srcRow = src + static_cast<size_t>(row) * static_cast<size_t>(stridePixels);
            uint16_t *dstRow = g_swapBuffer.data() + static_cast<size_t>(row) * static_cast<size_t>(w);
            for (int16_t col = 0; col < w; ++col)
                dstRow[col] = swap565(srcRow[col]);
        }

        pipcore::desktop::Runtime::instance().writeRect565(x, y, w, h, g_swapBuffer.data(), w);
    }

    uint16_t pcDisplayWidth()
    {
        return g_pcWidth;
    }

    uint16_t pcDisplayHeight()
    {
        return g_pcHeight;
    }
}

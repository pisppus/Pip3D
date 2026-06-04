#include "PcDisplayBlit.h"

namespace pip3D
{
    namespace
    {
        PcBlitCallback g_pcBlitCallback = nullptr;
        uint16_t g_pcWidth = 0;
        uint16_t g_pcHeight = 0;
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
        return g_pcBlitCallback != nullptr;
    }

    void blitPcDisplay(int16_t x,
                       int16_t y,
                       int16_t w,
                       int16_t h,
                       const uint16_t *src,
                       int32_t stridePixels)
    {
        if (!g_pcBlitCallback || !src || w <= 0 || h <= 0 || stridePixels < w)
            return;
        g_pcBlitCallback(x, y, w, h, src, stridePixels);
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

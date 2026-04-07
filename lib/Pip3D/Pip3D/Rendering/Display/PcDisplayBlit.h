#pragma once

#include "Core/Core.h"

namespace pip3D
{
    typedef void (*PcBlitCallback)(int16_t x,
                                   int16_t y,
                                   int16_t w,
                                   int16_t h,
                                   const uint16_t *src,
                                   int32_t stridePixels);

    void setPcBlitCallback(PcBlitCallback cb);
    PcBlitCallback getPcBlitCallback();
    bool initPcDisplay(const DisplayConfig &config);
    void blitPcDisplay(int16_t x,
                       int16_t y,
                       int16_t w,
                       int16_t h,
                       const uint16_t *src,
                       int32_t stridePixels);
    uint16_t pcDisplayWidth();
    uint16_t pcDisplayHeight();
}

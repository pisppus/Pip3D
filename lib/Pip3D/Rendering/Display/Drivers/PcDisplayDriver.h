#ifndef PCDISPLAYDRIVER_H
#define PCDISPLAYDRIVER_H

#include "DisplayConfig.h"
#include "DisplayDriverBase.h"

namespace pip3D
{

    typedef void (*PcBlitCallback)(int16_t x,
                                   int16_t y,
                                   int16_t w,
                                   int16_t h,
                                   const uint16_t *src);

    void setPcBlitCallback(PcBlitCallback cb);

    PcBlitCallback getPcBlitCallback();

    DisplayDriverBase *createPcDisplayDriver();

}

#endif

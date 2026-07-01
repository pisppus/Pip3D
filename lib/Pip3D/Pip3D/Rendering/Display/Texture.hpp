#pragma once

#include <stdint.h>

namespace pip3D
{
    struct Texture
    {
        const uint16_t *data;
        uint8_t widthShift;
        uint8_t heightShift;
        uint16_t widthMask;
        uint16_t heightMask;
        const uint16_t *palette;
        const uint16_t *mipData;
        uint8_t mipCount;
    };
}
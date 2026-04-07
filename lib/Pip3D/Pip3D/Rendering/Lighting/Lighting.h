#pragma once

#include "Core/Core.h"

namespace pip3D
{

    enum LightType
    {
        LIGHT_DIRECTIONAL,
        LIGHT_POINT
    };

    struct Light
    {
        static constexpr float RGB565_RED_TO_FLOAT = 0.03225806451612903226f;
        static constexpr float RGB565_GREEN_TO_FLOAT = 0.01587301587301587302f;
        static constexpr float RGB565_BLUE_TO_FLOAT = 0.03225806451612903226f;

        LightType type;
        float intensity;
        Vector3 direction;
        Vector3 position;
        Color color;
        float range;
        float rangeSq;
        float invRangeSq;

        mutable float cachedR, cachedG, cachedB;
        mutable bool colorCacheDirty;

        Light() : type(LIGHT_DIRECTIONAL),
                  intensity(1.0f),
                  direction(0, -1, 0),
                  position(0, 10, 0),
                  color(Color::WHITE),
                  range(0.0f),
                  rangeSq(0.0f),
                  invRangeSq(0.0f),
                  cachedR(0.0f),
                  cachedG(0.0f),
                  cachedB(0.0f),
                  colorCacheDirty(true)
        {
            direction.normalize();
        }

        __attribute__((always_inline)) inline void getCachedRGB(float &r, float &g, float &b) const
        {
            if (unlikely(colorCacheDirty))
            {
                const uint16_t lightRGB = color.rgb565;
                cachedR = ((lightRGB >> 11) & 0x1F) * RGB565_RED_TO_FLOAT;
                cachedG = ((lightRGB >> 5) & 0x3F) * RGB565_GREEN_TO_FLOAT;
                cachedB = (lightRGB & 0x1F) * RGB565_BLUE_TO_FLOAT;
                colorCacheDirty = false;
            }
            r = cachedR;
            g = cachedG;
            b = cachedB;
        }

        __attribute__((always_inline)) inline void setRange(float r)
        {
            range = r;
            if (r > 0.0f)
            {
                float rsq = r * r;
                rangeSq = rsq;
                invRangeSq = 1.0f / rsq;
            }
            else
            {
                rangeSq = 0.0f;
                invRangeSq = 0.0f;
            }
        }
    };

}


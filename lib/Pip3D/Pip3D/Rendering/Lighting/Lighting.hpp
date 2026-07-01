#pragma once

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Math/Algebra.hpp"

namespace pip3D
{

    enum LightType
    {
        LIGHT_DIRECTIONAL,
        LIGHT_POINT
    };

    struct alignas(16) Light
    {
        static constexpr float INV_31 = 1.0f / 31.0f;
        static constexpr float INV_63 = 1.0f / 63.0f;

        Vector3 direction;
        Vector3 position;
        Color color;
        float intensity;
        float range;
        float rangeSq;
        float invRangeSq;
        mutable float cachedR, cachedG, cachedB;
        mutable bool colorCacheDirty;
        LightType type;

        Light() : direction(0, -1, 0),
                  position(0, 10, 0),
                  color(Color::WHITE),
                  intensity(0.0f),
                  range(0.0f),
                  rangeSq(0.0f),
                  invRangeSq(0.0f),
                  cachedR(0.0f),
                  cachedG(0.0f),
                  cachedB(0.0f),
                  colorCacheDirty(true),
                  type(LIGHT_DIRECTIONAL)
        {
            direction.normalize();
        }

        __attribute__((always_inline)) inline void warmCache() const
        {
            if (unlikely(colorCacheDirty))
            {
                const uint16_t lightRGB = color.rgb565;
                cachedR = static_cast<float>((lightRGB >> 11) & 0x1F) * INV_31;
                cachedG = static_cast<float>((lightRGB >> 5) & 0x3F) * INV_63;
                cachedB = static_cast<float>(lightRGB & 0x1F) * INV_31;
                colorCacheDirty = false;
            }
        }

        __attribute__((always_inline)) inline void setRange(float r)
        {
            range = r;
            if (r > 0.0f)
            {
                const float rsq = r * r;
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

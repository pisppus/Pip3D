#pragma once

#include <float.h>

#include "Math/Collision.hpp"

namespace pip3D
{
    struct BuoyancyZone
    {
        AABB bounds;

        float surfaceLevel;
        float density;
        float dragLinear;
        float dragAngular;

        BuoyancyZone()
            : bounds(Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX),
                     Vector3(FLT_MAX, FLT_MAX, FLT_MAX)),
              surfaceLevel(0.0f),
              density(1.5f),
              dragLinear(2.0f),
              dragAngular(2.0f) {}

        BuoyancyZone(const AABB &bounds_,
                     float surface,
                     float density_ = 1.5f,
                     float dragL = 2.0f,
                     float dragA = 2.0f)
            : bounds(bounds_),
              surfaceLevel(surface),
              density(density_),
              dragLinear(dragL),
              dragAngular(dragA) {}

        PIP3D_FORCE_INLINE bool contains(const Vector3 &p) const noexcept
        {
            return bounds.contains(p);
        }
    };
}

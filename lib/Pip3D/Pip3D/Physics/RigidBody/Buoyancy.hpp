#pragma once

#include <float.h>

#include "Core/Platform.hpp"
#include "Math/Collision.hpp"
#include "Physics/RigidBody/Body.hpp"

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

    PIP3D_FORCE_INLINE void applyBuoyancy(RigidBody **bodies,
                                          size_t bodyCount,
                                          const BuoyancyZone *zones,
                                          size_t zoneCount,
                                          float effectiveGravity,
                                          float) noexcept
    {
        if (zoneCount == 0)
            return;

        for (size_t i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies[i];
            if (!b || b->isStatic || b->isKinematic || b->isSleeping)
                continue;
            if (b->getInvMass() <= 0.0f)
                continue;
            if (b->shape == BODY_SHAPE_SPHERE)
                continue;

            const Vector3 half = b->size * 0.5f;
            const Vector3 localCorners[4] = {
                Vector3(-half.x, -half.y, -half.z),
                Vector3(half.x, -half.y, -half.z),
                Vector3(half.x, -half.y, half.z),
                Vector3(-half.x, -half.y, half.z)};

            for (size_t zi = 0; zi < zoneCount; ++zi)
            {
                const BuoyancyZone &zone = zones[zi];
                bool anySubmerged = false;

                for (int c = 0; c < 4; ++c)
                {
                    const Vector3 worldCorner =
                        b->orientation.rotate(localCorners[c]) + b->position;
                    if (!zone.bounds.contains(worldCorner))
                        continue;

                    const float depth = zone.surfaceLevel - worldCorner.y;
                    if (depth <= 0.0f)
                        continue;
                    anySubmerged = true;

                    const float hRef = (b->size.y > 0.0f) ? b->size.y : 1.0f;
                    float depthFactor = depth / hRef;
                    if (depthFactor > 1.0f)
                        depthFactor = 1.0f;

                    const float cornerForceMag =
                        (b->getMass() * zone.density * effectiveGravity * 0.25f) *
                        depthFactor;
                    const Vector3 forceVec(0.0f, cornerForceMag, 0.0f);

                    b->applyForceAt(forceVec, worldCorner);
                }

                if (anySubmerged)
                {

                    float linFactor = 1.0f - zone.dragLinear * 0.016f;
                    float angFactor = 1.0f - zone.dragAngular * 0.016f;
                    if (linFactor < 0.0f)
                        linFactor = 0.0f;
                    if (angFactor < 0.0f)
                        angFactor = 0.0f;
                    b->velocity *= linFactor;
                    b->angularVelocity *= angFactor;
                }
            }
        }
    }
}

#pragma once

#include <cmath>
#include <float.h>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Math/Collision.hpp"
#include "Physics/RigidBody/Body.hpp"
#include "Physics/RigidBody/Contacts.hpp"
#include "Physics/Collision/Simplex.hpp"

namespace pip3D
{
    inline bool gjkIntersect(const RigidBody *a, const RigidBody *b,
                             GJKVertex *outSimplex, int &outSize) noexcept
    {
        const float bodyScale = a->radius + b->radius;
        const float safeScale = (bodyScale > 1e-4f) ? bodyScale : 1e-4f;
        const float kDistEps = 1e-5f * safeScale;
        const float kDupEpsSq = 1e-8f * safeScale * safeScale;

        Vector3 dir = b->position - a->position;
        if (dir.lengthSquared() < 1e-12f)
            dir = Vector3(0.0f, 1.0f, 0.0f);

        GJKVertex simplexBuf[4];
        int size = 0;

        Vector3 sa = a->support(dir);
        Vector3 sb = b->support(-dir);
        simplexBuf[0].m = sa - sb;
        simplexBuf[0].pA = sa;
        simplexBuf[0].pB = sb;
        size = 1;

        dir = -simplexBuf[0].m;

        const int kMaxGJKIters = 32;
        for (int iter = 0; iter < kMaxGJKIters; ++iter)
        {
            const float dirLenSq = dir.lengthSquared();
            if (dirLenSq < kDistEps * kDistEps)
            {

                for (int i = 0; i < size; ++i)
                    outSimplex[i] = simplexBuf[i];
                outSize = size;
                return true;
            }

            const float invDirLen = FastMath::fastInvSqrt(dirLenSq);
            dir = dir * invDirLen;

            sa = a->support(dir);
            sb = b->support(-dir);
            const Vector3 p = sa - sb;

            if (p.dot(dir) < -1e-5f)
            {
                outSize = 0;
                return false;
            }

            bool duplicate = false;
            for (int i = 0; i < size; ++i)
            {
                if ((p - simplexBuf[i].m).lengthSquared() < kDupEpsSq)
                {
                    duplicate = true;
                    break;
                }
            }

            if (duplicate)
            {

                GJKVertex tmp[4];
                for (int i = 0; i < size; ++i)
                    tmp[i] = simplexBuf[i];
                int tmpSize = size;
                Vector3 tmpDir;
                const bool containsOrigin = simplexClosestToOrigin(tmp, tmpSize, tmpDir);
                if (containsOrigin || tmpDir.lengthSquared() < kDistEps * kDistEps)
                {
                    for (int i = 0; i < tmpSize; ++i)
                        outSimplex[i] = tmp[i];
                    outSize = tmpSize;
                    return true;
                }
                outSize = 0;
                return false;
            }

            simplexBuf[size].m = p;
            simplexBuf[size].pA = sa;
            simplexBuf[size].pB = sb;
            ++size;

            Vector3 newDir;
            const bool containsOrigin = simplexClosestToOrigin(simplexBuf, size, newDir);
            if (containsOrigin)
            {
                for (int i = 0; i < size; ++i)
                    outSimplex[i] = simplexBuf[i];
                outSize = size;
                return true;
            }
            dir = newDir;
        }

        outSize = 0;
        return false;
    }
}
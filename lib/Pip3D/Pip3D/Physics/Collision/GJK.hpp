#pragma once

#include <math.h>
#include <float.h>

#include "Math/Algebra.hpp"
#include "../Dynamics/Body.hpp"
#include "../Dynamics/Contacts.hpp"
#include "GJKTypes.hpp"
#include "Simplex.hpp"

namespace pip3D
{

    inline bool gjkIntersect(const RigidBody *a, const RigidBody *b,
                             GJKVertex *outSimplex, int &outSize)
    {

        const float bodyScale = a->radius + b->radius;
        const float kDistEps = 1e-5f * fmaxf(bodyScale, 1.0f);
        const float kDupEps = 1e-6f * fmaxf(bodyScale * bodyScale, 1.0f);

        Vector3 dir = v3sub(b->position, a->position);
        if (v3dot(dir, dir) < 1e-12f)
            dir = Vector3(1, 0, 0);

        GJKVertex simplexBuf[4];
        int size = 0;

        Vector3 sa = a->support(dir);
        Vector3 sb = b->support(v3neg(dir));
        simplexBuf[0].m = v3sub(sa, sb);
        simplexBuf[0].pA = sa;
        simplexBuf[0].pB = sb;
        size = 1;

        dir = v3neg(simplexBuf[0].m);

        const int kMaxGJKIters = 32;
        for (int iter = 0; iter < kMaxGJKIters; ++iter)
        {
            float dirLen = sqrtf(v3dot(dir, dir));
            if (dirLen < kDistEps)
            {
                for (int i = 0; i < size; ++i)
                    outSimplex[i] = simplexBuf[i];
                outSize = size;
                return true;
            }
            dir = v3scale(dir, 1.0f / dirLen);

            sa = a->support(dir);
            sb = b->support(v3neg(dir));
            Vector3 p = v3sub(sa, sb);

            float proj = v3dot(p, dir);
            if (proj < -kDistEps)
            {
                outSize = 0;
                return false;
            }

            bool duplicate = false;
            for (int i = 0; i < size; ++i)
            {
                Vector3 d = v3sub(p, simplexBuf[i].m);
                if (v3dot(d, d) < kDupEps)
                {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate)
            {

                Vector3 tmpDir;
                GJKVertex tmp[4];
                for (int i = 0; i < size; ++i)
                    tmp[i] = simplexBuf[i];
                int tmpSize = size;
                simplexClosestToOrigin(tmp, tmpSize, tmpDir);
                float dSq = v3dot(tmpDir, tmpDir);
                if (dSq < kDistEps * kDistEps)
                {
                    for (int i = 0; i < tmpSize; ++i)
                        outSimplex[i] = tmp[i];
                    outSize = tmpSize;
                    return true;
                }
                outSize = 0;
                return false;
            }

            if (size >= 4)
            {
                outSize = 0;
                return false;
            }
            simplexBuf[size].m = p;
            simplexBuf[size].pA = sa;
            simplexBuf[size].pB = sb;
            ++size;

            Vector3 newDir;
            bool containsOrigin = simplexClosestToOrigin(simplexBuf, size, newDir);
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

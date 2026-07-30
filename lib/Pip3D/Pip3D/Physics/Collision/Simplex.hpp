#pragma once

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"

namespace pip3D
{
    struct GJKVertex
    {
        Vector3 m;
        Vector3 pA;
        Vector3 pB;
    };

    PIP3D_FORCE_INLINE bool isOutsideFace(const Vector3 &A, const Vector3 &B, const Vector3 &C, const Vector3 &D) noexcept
    {
        const Vector3 normal = (B - A).cross(C - A);
        const float signD = normal.dot(D - A);
        const float signO = normal.dot(-A);
        return (signD * signO < -1e-6f);
    }

    inline bool simplexClosestToOrigin(GJKVertex *simplex, int &size, Vector3 &newDir) noexcept
    {
        if (size == 1)
        {
            newDir = -simplex[0].m;
            return false;
        }

        if (size == 2)
        {

            const GJKVertex A_vert = simplex[1];
            const GJKVertex B_vert = simplex[0];
            const Vector3 A = A_vert.m;
            const Vector3 B = B_vert.m;
            const Vector3 AB = B - A;
            const Vector3 AO = -A;

            const float abLenSq = AB.lengthSquared();
            if (abLenSq < 1e-12f)
            {
                simplex[0] = A_vert;
                size = 1;
                newDir = -A;
                return false;
            }

            const float t = AO.dot(AB) / abLenSq;
            if (t <= 0.0f)
            {
                simplex[0] = A_vert;
                size = 1;
                newDir = -A;
                return false;
            }
            if (t >= 1.0f)
            {
                size = 1;
                newDir = -B;
                return false;
            }

            newDir = -(A + AB * t);
            return false;
        }

        if (size == 3)
        {

            const GJKVertex A_vert = simplex[2];
            const GJKVertex B_vert = simplex[1];
            const GJKVertex C_vert = simplex[0];
            const Vector3 A = A_vert.m;
            const Vector3 B = B_vert.m;
            const Vector3 C = C_vert.m;

            const Vector3 AB = B - A;
            const Vector3 AC = C - A;
            const Vector3 AP = -A;

            const float d1 = AB.dot(AP);
            const float d2 = AC.dot(AP);
            if (d1 <= 0.0f && d2 <= 0.0f)
            {
                simplex[0] = A_vert;
                size = 1;
                newDir = -A;
                return false;
            }

            const Vector3 BP = -B;
            const float d3 = AB.dot(BP);
            const float d4 = AC.dot(BP);
            if (d3 >= 0.0f && d4 <= d3)
            {
                simplex[0] = B_vert;
                size = 1;
                newDir = -B;
                return false;
            }

            const float vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
            {
                const float denom = d1 - d3;
                const float t = d1 / denom;
                simplex[0] = B_vert;
                simplex[1] = A_vert;
                size = 2;
                newDir = -(A + AB * t);
                return false;
            }

            const Vector3 CP = -C;
            const float d5 = AB.dot(CP);
            const float d6 = AC.dot(CP);
            if (d6 >= 0.0f && d5 <= d6)
            {
                simplex[0] = C_vert;
                size = 1;
                newDir = -C;
                return false;
            }

            const float vb = d5 * d2 - d1 * d6;
            if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
            {
                const float denom = d2 - d6;
                const float t = d2 / denom;
                simplex[0] = C_vert;
                simplex[1] = A_vert;
                size = 2;
                newDir = -(A + AC * t);
                return false;
            }

            const float va = d3 * d6 - d5 * d4;
            if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
            {
                const float denom = (d4 - d3) + (d5 - d6);
                const float t = (d4 - d3) / denom;
                simplex[0] = C_vert;
                simplex[1] = B_vert;
                size = 2;
                newDir = -(B + (C - B) * t);
                return false;
            }

            const float denom = 1.0f / (va + vb + vc);
            const float v = vb * denom;
            const float w = vc * denom;
            const Vector3 closest = A + AB * v + AC * w;
            newDir = -closest;
            return false;
        }

        if (size == 4)
        {

            const GJKVertex A_vert = simplex[3];
            const GJKVertex B_vert = simplex[2];
            const GJKVertex C_vert = simplex[1];
            const GJKVertex D_vert = simplex[0];
            const Vector3 A = A_vert.m;
            const Vector3 B = B_vert.m;
            const Vector3 C = C_vert.m;
            const Vector3 D = D_vert.m;

            if (isOutsideFace(A, B, C, D))
            {
                simplex[0] = C_vert;
                simplex[1] = B_vert;
                simplex[2] = A_vert;
                size = 3;
                return simplexClosestToOrigin(simplex, size, newDir);
            }

            if (isOutsideFace(A, C, D, B))
            {
                simplex[0] = D_vert;
                simplex[1] = C_vert;
                simplex[2] = A_vert;
                size = 3;
                return simplexClosestToOrigin(simplex, size, newDir);
            }

            if (isOutsideFace(A, D, B, C))
            {
                simplex[0] = B_vert;
                simplex[1] = D_vert;
                simplex[2] = A_vert;
                size = 3;
                return simplexClosestToOrigin(simplex, size, newDir);
            }

            return true;
        }

        return false;
    }
}
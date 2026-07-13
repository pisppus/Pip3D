#pragma once

#include "GJKTypes.hpp"

namespace pip3D
{
    inline bool simplexClosestToOrigin(GJKVertex *simplex, int &size, Vector3 &newDir)
    {

        if (size == 1)
        {
            newDir = v3neg(simplex[0].m);
            return false;
        }

        if (size == 2)
        {
            Vector3 A = simplex[1].m;
            Vector3 B = simplex[0].m;
            Vector3 AB = v3sub(B, A);
            float abLenSq = v3dot(AB, AB);
            if (abLenSq < 1e-12f)
            {
                simplex[0] = simplex[1];
                size = 1;
                newDir = v3neg(A);
                return false;
            }
            float t = v3dot(v3neg(A), AB) / abLenSq;
            if (t <= 0.0f)
            {
                simplex[0] = simplex[1];
                size = 1;
                newDir = v3neg(A);
                return false;
            }
            if (t >= 1.0f)
            {
                size = 1;
                newDir = v3neg(B);
                return false;
            }

            Vector3 closest = v3add(A, v3scale(AB, t));
            newDir = v3neg(closest);
            if (v3dot(newDir, newDir) < 1e-14f)
                return true;
            return false;
        }

        if (size == 3)
        {
            Vector3 A = simplex[2].m;
            Vector3 B = simplex[1].m;
            Vector3 C = simplex[0].m;
            Vector3 AB = v3sub(B, A);
            Vector3 AC = v3sub(C, A);
            Vector3 AP = v3neg(A);

            float d1 = v3dot(AB, AP);
            float d2 = v3dot(AC, AP);
            if (d1 <= 0.0f && d2 <= 0.0f)
            {
                simplex[0] = simplex[2];
                size = 1;
                newDir = v3neg(A);
                return false;
            }

            Vector3 BP = v3neg(B);
            float d3 = v3dot(AB, BP);
            float d4 = v3dot(AC, BP);
            if (d3 >= 0.0f && d4 <= d3)
            {
                simplex[0] = simplex[1];
                size = 1;
                newDir = v3neg(B);
                return false;
            }

            float vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
            {
                float t = d1 / (d1 - d3);
                Vector3 closest = v3add(A, v3scale(AB, t));
                simplex[0] = simplex[1];
                simplex[1] = simplex[2];
                size = 2;
                newDir = v3neg(closest);
                if (v3dot(newDir, newDir) < 1e-14f)
                    return true;
                return false;
            }

            Vector3 CP = v3neg(C);
            float d5 = v3dot(AB, CP);
            float d6 = v3dot(AC, CP);
            if (d6 >= 0.0f && d5 <= d6)
            {
                size = 1;
                newDir = v3neg(C);
                return false;
            }

            float vb = d5 * d2 - d1 * d6;
            if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
            {
                float t = d2 / (d2 - d6);
                Vector3 closest = v3add(A, v3scale(AC, t));
                simplex[1] = simplex[2];
                size = 2;
                newDir = v3neg(closest);
                if (v3dot(newDir, newDir) < 1e-14f)
                    return true;
                return false;
            }

            float va = d3 * d6 - d5 * d4;
            if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
            {
                float t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                Vector3 closest = v3add(B, v3scale(v3sub(C, B), t));
                simplex[2] = simplex[1];
                size = 2;
                newDir = v3neg(closest);
                if (v3dot(newDir, newDir) < 1e-14f)
                    return true;
                return false;
            }

            Vector3 n = v3cross(AB, AC);
            float nLenSq = v3dot(n, n);
            if (nLenSq < 1e-14f)
            {
                simplex[0] = simplex[2];
                size = 1;
                newDir = v3neg(A);
                return false;
            }
            float signedDist = v3dot(n, A);
            if (fabsf(signedDist) < 1e-3f * sqrtf(nLenSq))
                return true;
            if (signedDist > 0.0f)
                newDir = v3neg(n);
            else
                newDir = n;
            float dirLen = sqrtf(v3dot(newDir, newDir));
            if (dirLen > 1e-12f)
                newDir = v3scale(newDir, 1.0f / dirLen);
            return false;
        }

        Vector3 A = simplex[3].m;
        Vector3 B = simplex[2].m;
        Vector3 C = simplex[1].m;
        Vector3 D = simplex[0].m;

        auto signedVol = [](const Vector3 &a, const Vector3 &b,
                            const Vector3 &c, const Vector3 &d) -> float
        {
            Vector3 ab = v3sub(b, a);
            Vector3 ac = v3sub(c, a);
            Vector3 ad = v3sub(d, a);
            return v3dot(v3cross(ab, ac), ad);
        };

        Vector3 O(0, 0, 0);
        float volABCD = signedVol(A, B, C, D);
        float volOBCD = signedVol(O, B, C, D);
        float volAOCD = signedVol(A, O, C, D);
        float volABOD = signedVol(A, B, O, D);
        float volABCO = signedVol(A, B, C, O);

        if (fabsf(volABCD) < 1e-10f)
        {
            simplex[0] = simplex[1];
            simplex[1] = simplex[2];
            simplex[2] = simplex[3];
            size = 3;
            return simplexClosestToOrigin(simplex, size, newDir);
        }

        bool signMain = (volABCD > 0.0f);
        bool sameSign = ((volOBCD > 0.0f) == signMain) &&
                        ((volAOCD > 0.0f) == signMain) &&
                        ((volABOD > 0.0f) == signMain) &&
                        ((volABCO > 0.0f) == signMain);
        if (sameSign)
            return true;

        struct FaceCand
        {
            int dropIdx;
            float mag;
            bool wrongSign;
        };
        FaceCand cands[4] = {
            {3, fabsf(volOBCD), (volOBCD > 0.0f) != signMain},
            {2, fabsf(volAOCD), (volAOCD > 0.0f) != signMain},
            {1, fabsf(volABOD), (volABOD > 0.0f) != signMain},
            {0, fabsf(volABCO), (volABCO > 0.0f) != signMain},
        };
        int bestDrop = -1;
        float bestMag = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            if (cands[i].wrongSign && cands[i].mag > bestMag)
            {
                bestMag = cands[i].mag;
                bestDrop = cands[i].dropIdx;
            }
        }
        if (bestDrop < 0)
            return true;

        GJKVertex keep[3];
        int k = 0;
        for (int i = 0; i < 4; ++i)
            if (i != bestDrop)
                keep[k++] = simplex[i];
        simplex[0] = keep[0];
        simplex[1] = keep[1];
        simplex[2] = keep[2];
        size = 3;
        return simplexClosestToOrigin(simplex, size, newDir);
    }
}

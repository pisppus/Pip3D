#pragma once

#include "GJK.hpp"
#include "Simplex.hpp"

namespace pip3D
{
    struct EPAResult
    {
        Vector3 normal;
        float depth;
        Vector3 contactPointA;
        Vector3 contactPointB;
    };

    struct EPAFace
    {
        int a, b, c;
        Vector3 normal;
        float dist;
    };
    inline bool epaPenetration(const RigidBody *a, const RigidBody *b,
                               const GJKVertex *gjkSimplex, int gjkSize,
                               EPAResult &out)
    {
        const float bodyScale = a->radius + b->radius;
        const float kEps = 1e-5f * fmaxf(bodyScale, 1.0f);

        GJKVertex verts[64];
        int vertCount = 0;

        for (int i = 0; i < gjkSize && vertCount < 4; ++i)
            verts[vertCount++] = gjkSimplex[i];

        if (vertCount < 4)
        {
            const Vector3 kExtraDirs[6] = {
                Vector3(1, 0, 0), Vector3(-1, 0, 0),
                Vector3(0, 1, 0), Vector3(0, -1, 0),
                Vector3(0, 0, 1), Vector3(0, 0, -1)};
            for (int i = 0; i < 6 && vertCount < 4; ++i)
            {
                Vector3 sa = a->support(kExtraDirs[i]);
                Vector3 sb = b->support(v3neg(kExtraDirs[i]));
                GJKVertex v;
                v.m = v3sub(sa, sb);
                v.pA = sa;
                v.pB = sb;
                bool dup = false;
                for (int j = 0; j < vertCount; ++j)
                {
                    Vector3 d = v3sub(v.m, verts[j].m);
                    if (v3dot(d, d) < 1e-10f)
                    {
                        dup = true;
                        break;
                    }
                }
                if (!dup)
                    verts[vertCount++] = v;
            }
            if (vertCount < 4)
                return false;
        }

        EPAFace faces[128];
        int faceCount = 0;

        auto addFace = [&](int i0, int i1, int i2)
        {
            if (faceCount >= 128)
                return;
            Vector3 A = verts[i0].m;
            Vector3 B = verts[i1].m;
            Vector3 C = verts[i2].m;
            Vector3 n = v3cross(v3sub(B, A), v3sub(C, A));
            float nLen = v3len(n);
            if (nLen < 1e-10f)
                return;
            n = v3scale(n, 1.0f / nLen);
            float d = v3dot(n, A);
            if (d < 0.0f)
            {
                n = v3neg(n);
                d = -d;
            }
            EPAFace f;
            f.a = i0;
            f.b = i1;
            f.c = i2;
            f.normal = n;
            f.dist = d;
            faces[faceCount++] = f;
        };

        addFace(1, 2, 3);
        addFace(0, 3, 2);
        addFace(0, 1, 3);
        addFace(0, 2, 1);

        const int kMaxEPAIters = 32;

        for (int iter = 0; iter < kMaxEPAIters; ++iter)
        {

            int bestFace = 0;
            float bestDist = faces[0].dist;
            for (int i = 1; i < faceCount; ++i)
            {
                if (faces[i].dist < bestDist)
                {
                    bestDist = faces[i].dist;
                    bestFace = i;
                }
            }

            Vector3 dir = faces[bestFace].normal;
            Vector3 sa = a->support(dir);
            Vector3 sb = b->support(v3neg(dir));
            GJKVertex newV;
            newV.m = v3sub(sa, sb);
            newV.pA = sa;
            newV.pB = sb;

            float supportDist = v3dot(newV.m, dir);
            if (supportDist - bestDist < kEps)
            {

                EPAFace &f = faces[bestFace];
                Vector3 A = verts[f.a].m;
                Vector3 B = verts[f.b].m;
                Vector3 C = verts[f.c].m;

                Vector3 AB = v3sub(B, A);
                Vector3 AC = v3sub(C, A);
                Vector3 AP = v3neg(A);
                float d1 = v3dot(AB, AP);
                float d2 = v3dot(AC, AP);

                Vector3 BP = v3neg(B);
                float d3 = v3dot(AB, BP);
                float d4 = v3dot(AC, BP);

                Vector3 CP = v3neg(C);
                float d5 = v3dot(AB, CP);
                float d6 = v3dot(AC, CP);

                float va = d3 * d6 - d5 * d4;
                float vb = d5 * d2 - d1 * d6;
                float vc = d1 * d4 - d3 * d2;

                float denom = va + vb + vc;
                if (fabsf(denom) < 1e-12f)
                {

                    out.normal = f.normal;
                    out.depth = bestDist;
                    out.contactPointA = verts[f.a].pA;
                    out.contactPointB = verts[f.a].pB;
                    return true;
                }
                float invDenom = 1.0f / denom;
                float u = va * invDenom;
                float v = vb * invDenom;
                float w = vc * invDenom;

                out.normal = f.normal;
                out.depth = bestDist;
                out.contactPointA = v3add(v3add(v3scale(verts[f.a].pA, u),
                                                v3scale(verts[f.b].pA, v)),
                                          v3scale(verts[f.c].pA, w));
                out.contactPointB = v3add(v3add(v3scale(verts[f.a].pB, u),
                                                v3scale(verts[f.b].pB, v)),
                                          v3scale(verts[f.c].pB, w));
                return true;
            }

            if (vertCount >= 64)
                break;
            int newIdx = vertCount;
            verts[vertCount++] = newV;

            bool visible[128];
            for (int i = 0; i < faceCount; ++i)
            {
                Vector3 fv = verts[faces[i].a].m;
                Vector3 toNew = v3sub(newV.m, fv);
                visible[i] = (v3dot(faces[i].normal, toNew) > 0.0f);
            }

            auto edgeInVisibleFace = [&](int i, int j) -> bool
            {
                for (int f = 0; f < faceCount; ++f)
                {
                    if (!visible[f])
                        continue;
                    int a_ = faces[f].a, b_ = faces[f].b, c_ = faces[f].c;
                    if ((a_ == j && b_ == i) || (b_ == j && c_ == i) || (c_ == j && a_ == i))
                        return true;
                }
                return false;
            };

            EPAFace newFaces[64];
            int newFaceCount = 0;
            for (int i = 0; i < faceCount; ++i)
            {
                if (visible[i])
                    continue;
                int ai = faces[i].a, bi = faces[i].b, ci = faces[i].c;

                if (edgeInVisibleFace(ai, bi))
                {
                    EPAFace nf;
                    nf.a = ai;
                    nf.b = bi;
                    nf.c = newIdx;
                    newFaces[newFaceCount++] = nf;
                }
                if (edgeInVisibleFace(bi, ci))
                {
                    EPAFace nf;
                    nf.a = bi;
                    nf.b = ci;
                    nf.c = newIdx;
                    newFaces[newFaceCount++] = nf;
                }
                if (edgeInVisibleFace(ci, ai))
                {
                    EPAFace nf;
                    nf.a = ci;
                    nf.b = ai;
                    nf.c = newIdx;
                    newFaces[newFaceCount++] = nf;
                }
            }

            int writeIdx = 0;
            for (int i = 0; i < faceCount; ++i)
                if (!visible[i])
                    faces[writeIdx++] = faces[i];
            faceCount = writeIdx;

            for (int i = 0; i < newFaceCount; ++i)
            {
                if (faceCount >= 128)
                    break;
                EPAFace &nf = newFaces[i];
                Vector3 A = verts[nf.a].m;
                Vector3 B = verts[nf.b].m;
                Vector3 C = verts[nf.c].m;
                Vector3 n = v3cross(v3sub(B, A), v3sub(C, A));
                float nLen = v3len(n);
                if (nLen < 1e-10f)
                    continue;
                n = v3scale(n, 1.0f / nLen);
                float d = v3dot(n, A);
                if (d < 0.0f)
                {
                    n = v3neg(n);
                    d = -d;
                }
                nf.normal = n;
                nf.dist = d;
                faces[faceCount++] = nf;
            }
        }

        if (faceCount == 0)
            return false;
        int bestFace = 0;
        float bestDist = faces[0].dist;
        for (int i = 1; i < faceCount; ++i)
            if (faces[i].dist < bestDist)
            {
                bestDist = faces[i].dist;
                bestFace = i;
            }
        out.normal = faces[bestFace].normal;
        out.depth = bestDist;
        out.contactPointA = verts[faces[bestFace].a].pA;
        out.contactPointB = verts[faces[bestFace].a].pB;
        return true;
    }
}

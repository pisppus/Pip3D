#pragma once

#include <utility>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Physics/Collision/Simplex.hpp"

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

    struct EPAEdge
    {
        int v0, v1;
    };

    inline bool epaPenetration(const RigidBody *a, const RigidBody *b,
                               const GJKVertex *gjkSimplex, int gjkSize,
                               EPAResult &out) noexcept
    {
        if (gjkSize < 4)
            return false;

        const float bodyScale = a->radius + b->radius;
        const float safeScale = (bodyScale > 1e-4f) ? bodyScale : 1e-4f;
        const float kEps = 1e-5f * safeScale;
        const float kDegenerateNormalSq = 1e-10f * safeScale * safeScale;
        const float kDupEpsSq = 1e-8f * safeScale * safeScale;

        static DRAM_ATTR GJKVertex verts[64];
        static DRAM_ATTR EPAFace faces[128];
        static DRAM_ATTR bool visible[128];
        static DRAM_ATTR EPAEdge horizon[128];

        int vertCount = 4;

        for (int i = 0; i < 4; ++i)
            verts[i] = gjkSimplex[i];

        const Vector3 v0 = verts[0].m;
        const Vector3 v1 = verts[1].m;
        const Vector3 v2 = verts[2].m;
        const Vector3 v3 = verts[3].m;
        const float vol = (v1 - v0).dot((v2 - v0).cross(v3 - v0));
        if (vol < 0.0f)
        {
            std::swap(verts[1], verts[2]);
        }

        int faceCount = 0;

        auto addFace = [&](int i0, int i1, int i2)
        {
            if (faceCount >= 128)
                return;
            const Vector3 A = verts[i0].m;
            const Vector3 B = verts[i1].m;
            const Vector3 C = verts[i2].m;
            Vector3 n = (B - A).cross(C - A);
            const float nLenSq = n.lengthSquared();
            if (nLenSq < kDegenerateNormalSq)
                return;
            const float nLen = sqrtf(nLenSq);
            n = n * (1.0f / nLen);
            float d = n.dot(A);
            if (d < 0.0f)
            {
                n = -n;
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

        addFace(0, 2, 1);
        addFace(0, 1, 3);
        addFace(0, 3, 2);
        addFace(1, 2, 3);

        if (faceCount == 0)
            return false;

        const int kMaxEPAIters = 48;
        for (int iter = 0; iter < kMaxEPAIters; ++iter)
        {
            int bestFace = 0;
            float minDist = faces[0].dist;
            for (int i = 1; i < faceCount; ++i)
            {
                if (faces[i].dist < minDist)
                {
                    minDist = faces[i].dist;
                    bestFace = i;
                }
            }

            const Vector3 dir = faces[bestFace].normal;
            const Vector3 sa = a->support(dir);
            const Vector3 sb = b->support(-dir);
            GJKVertex newV;
            newV.m = sa - sb;
            newV.pA = sa;
            newV.pB = sb;

            const float supportDist = newV.m.dot(dir);
            if (supportDist - minDist < kEps)
            {

                const EPAFace &f = faces[bestFace];
                const Vector3 A = verts[f.a].m;
                const Vector3 B = verts[f.b].m;
                const Vector3 C = verts[f.c].m;

                const Vector3 Q = dir * minDist;
                const Vector3 v0_edge = B - A;
                const Vector3 v1_edge = C - A;
                const Vector3 v2_edge = Q - A;

                const float d00 = v0_edge.dot(v0_edge);
                const float d01 = v0_edge.dot(v1_edge);
                const float d11 = v1_edge.dot(v1_edge);
                const float d20 = v2_edge.dot(v0_edge);
                const float d21 = v2_edge.dot(v1_edge);

                const float denom = d00 * d11 - d01 * d01;
                float u = 1.0f, v = 0.0f, w = 0.0f;
                if (fabsf(denom) > 1e-10f)
                {
                    const float invDenom = 1.0f / denom;
                    v = (d11 * d20 - d01 * d21) * invDenom;
                    w = (d00 * d21 - d01 * d20) * invDenom;
                    u = 1.0f - v - w;
                }

                if (u < 0.0f)
                {
                    u = 0.0f;
                    float s = v + w;
                    if (s > 0.0f)
                    {
                        v /= s;
                        w /= s;
                    }
                }
                if (v < 0.0f)
                {
                    v = 0.0f;
                    float s = u + w;
                    if (s > 0.0f)
                    {
                        u /= s;
                        w /= s;
                    }
                }
                if (w < 0.0f)
                {
                    w = 0.0f;
                    float s = u + v;
                    if (s > 0.0f)
                    {
                        u /= s;
                        v /= s;
                    }
                }

                out.normal = dir;
                out.depth = minDist;
                out.contactPointA = verts[f.a].pA * u + verts[f.b].pA * v + verts[f.c].pA * w;
                out.contactPointB = verts[f.a].pB * u + verts[f.b].pB * v + verts[f.c].pB * w;
                return true;
            }

            if (vertCount >= 64)
                break;

            bool duplicate = false;
            for (int i = 0; i < vertCount; ++i)
            {
                if ((newV.m - verts[i].m).lengthSquared() < kDupEpsSq)
                {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate)
            {
                const EPAFace &f = faces[bestFace];
                out.normal = dir;
                out.depth = minDist;
                out.contactPointA = verts[f.a].pA;
                out.contactPointB = verts[f.a].pB;
                return true;
            }

            const int newIdx = vertCount;
            verts[vertCount++] = newV;

            for (int i = 0; i < faceCount; ++i)
            {
                const Vector3 toNew = newV.m - verts[faces[i].a].m;
                visible[i] = (faces[i].normal.dot(toNew) > 1e-5f);
            }

            int horizonCount = 0;

            for (int i = 0; i < faceCount; ++i)
            {
                if (!visible[i])
                    continue;

                const int f_edges[3][2] = {
                    {faces[i].a, faces[i].b},
                    {faces[i].b, faces[i].c},
                    {faces[i].c, faces[i].a}};

                for (int e = 0; e < 3; ++e)
                {
                    const int u_edge = f_edges[e][0];
                    const int v_edge = f_edges[e][1];
                    bool shared = false;

                    for (int other = 0; other < faceCount; ++other)
                    {
                        if (other == i || !visible[other])
                            continue;

                        if ((faces[other].a == v_edge && faces[other].b == u_edge) ||
                            (faces[other].b == v_edge && faces[other].c == u_edge) ||
                            (faces[other].c == v_edge && faces[other].a == u_edge))
                        {
                            shared = true;
                            break;
                        }
                    }

                    if (!shared && horizonCount < 128)
                    {
                        horizon[horizonCount++] = {u_edge, v_edge};
                    }
                }
            }

            int writeIdx = 0;
            for (int i = 0; i < faceCount; ++i)
            {
                if (!visible[i])
                {
                    faces[writeIdx++] = faces[i];
                }
            }
            faceCount = writeIdx;

            if (horizonCount == 0 || faceCount == 0)
                return false;

            for (int i = 0; i < horizonCount; ++i)
            {
                addFace(horizon[i].v0, horizon[i].v1, newIdx);
            }
        }

        int bestFace = 0;
        float minDist = faces[0].dist;
        for (int i = 1; i < faceCount; ++i)
        {
            if (faces[i].dist < minDist)
            {
                minDist = faces[i].dist;
                bestFace = i;
            }
        }
        const EPAFace &f = faces[bestFace];
        out.normal = f.normal;
        out.depth = minDist;
        out.contactPointA = verts[f.a].pA;
        out.contactPointB = verts[f.a].pB;
        return true;
    }
}
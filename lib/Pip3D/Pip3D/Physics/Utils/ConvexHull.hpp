#pragma once

#include <cfloat>

#include "Math/Algebra.hpp"
#include "Geometry/Mesh.hpp"

namespace pip3D
{

    inline Vector3 pickExtremeVertex(const Mesh &mesh, const Vector3 &dir,
                                     const Vector3 &center, const Vector3 &scale)
    {
        const uint16_t n = mesh.numVertices();
        if (n == 0)
            return Vector3(0, 0, 0);

        const Vertex *v = mesh.vertexData();
        float bestDot = -FLT_MAX;
        Vector3 bestPos(0, 0, 0);

        for (uint16_t i = 0; i < n; ++i)
        {
            const Vector3 p = mesh.decodePosition(v[i]);
            const Vector3 sp(p.x * scale.x, p.y * scale.y, p.z * scale.z);
            const Vector3 d = sp - center;
            const float dp = d.dot(dir);
            if (dp > bestDot)
            {
                bestDot = dp;
                bestPos = sp;
            }
        }
        return bestPos;
    }

    inline void extractConvexVertices(const Mesh &mesh, const Vector3 &scale,
                                      const Vector3 &hullOrigin,
                                      Vector3 *outVerts, int *outCount, int maxVerts)
    {
        const uint16_t n = mesh.numVertices();
        if (n == 0 || maxVerts <= 0)
        {
            *outCount = 0;
            return;
        }

        const Vertex *v = mesh.vertexData();
        constexpr int kMaxWorkVerts = 256;
        const uint16_t workN = (n > kMaxWorkVerts) ? kMaxWorkVerts : n;

        Vector3 work[kMaxWorkVerts];
        int workCount = 0;
        const float kDedupEpsSq = 1e-6f;

        for (uint16_t i = 0; i < workN; ++i)
        {
            Vector3 p = mesh.decodePosition(v[i]);
            Vector3 sp(p.x * scale.x - hullOrigin.x,
                       p.y * scale.y - hullOrigin.y,
                       p.z * scale.z - hullOrigin.z);

            bool dup = false;
            for (int j = 0; j < workCount; ++j)
            {
                const Vector3 d = sp - work[j];
                if (d.lengthSquared() < kDedupEpsSq)
                {
                    dup = true;
                    break;
                }
            }
            if (!dup && workCount < kMaxWorkVerts)
                work[workCount++] = sp;
        }

        if (workCount == 0)
        {
            *outCount = 0;
            return;
        }

        int pickedIdx[kMaxWorkVerts];
        int pickedCount = 0;
        bool used[kMaxWorkVerts] = {false};

        auto pickExtreme = [&](const Vector3 &dir) -> int
        {
            int best = -1;
            float bestDot = -FLT_MAX;
            for (int i = 0; i < workCount; ++i)
            {
                if (used[i])
                    continue;
                const float d = dir.dot(work[i]);
                if (d > bestDot)
                {
                    bestDot = d;
                    best = i;
                }
            }
            return best;
        };

        auto tryPick = [&](const Vector3 &dir)
        {
            if (pickedCount >= maxVerts)
                return;
            int idx = pickExtreme(dir);
            if (idx >= 0 && !used[idx])
            {
                used[idx] = true;
                pickedIdx[pickedCount++] = idx;
            }
        };

        tryPick(Vector3(1, 0, 0));
        tryPick(Vector3(-1, 0, 0));
        tryPick(Vector3(0, 1, 0));
        tryPick(Vector3(0, -1, 0));
        tryPick(Vector3(0, 0, 1));
        tryPick(Vector3(0, 0, -1));

        const float k = 0.57735026919f;
        tryPick(Vector3(k, k, k));
        tryPick(Vector3(-k, k, k));
        tryPick(Vector3(k, -k, k));
        tryPick(Vector3(-k, -k, k));
        tryPick(Vector3(k, k, -k));
        tryPick(Vector3(-k, k, -k));
        tryPick(Vector3(k, -k, -k));
        tryPick(Vector3(-k, -k, -k));

        for (int a = 0; a < 4; ++a)
        {
            const float ang = static_cast<float>(a) * 0.7853981f;
            float sa, ca;
            FastMath::fastSinCos(ang, sa, ca);
            tryPick(Vector3(ca * 0.3f, 1.0f, sa * 0.3f));
            tryPick(Vector3(ca * 0.3f, -1.0f, sa * 0.3f));
        }

        const int out = (pickedCount < maxVerts) ? pickedCount : maxVerts;
        for (int i = 0; i < out; ++i)
            outVerts[i] = work[pickedIdx[i]];
        *outCount = out;
    }

    inline void extractConvexVertices(const Mesh &mesh, const Vector3 &scale,
                                      Vector3 *outVerts, int *outCount, int maxVerts)
    {
        const uint16_t n = mesh.numVertices();
        if (n == 0 || maxVerts <= 0)
        {
            *outCount = 0;
            return;
        }

        const Vertex *v = mesh.vertexData();
        Vector3 sum(0, 0, 0);
        for (uint16_t i = 0; i < n; ++i)
        {
            const Vector3 p = mesh.decodePosition(v[i]);
            sum.x += p.x * scale.x;
            sum.y += p.y * scale.y;
            sum.z += p.z * scale.z;
        }
        const Vector3 center = sum * (1.0f / float(n));

        static const Vector3 kAxisDirs[6] = {
            Vector3(1, 0, 0), Vector3(-1, 0, 0),
            Vector3(0, 1, 0), Vector3(0, -1, 0),
            Vector3(0, 0, 1), Vector3(0, 0, -1)};

        static const Vector3 kCornerDirs[8] = {
            Vector3(1, 1, 1), Vector3(-1, 1, 1),
            Vector3(1, -1, 1), Vector3(-1, -1, 1),
            Vector3(1, 1, -1), Vector3(-1, 1, -1),
            Vector3(1, -1, -1), Vector3(-1, -1, -1)};

        Vector3 candidates[14];
        int candCount = 0;
        for (int i = 0; i < 6; ++i)
            candidates[candCount++] = pickExtremeVertex(mesh, kAxisDirs[i], center, scale);
        for (int i = 0; i < 8; ++i)
            candidates[candCount++] = pickExtremeVertex(mesh, kCornerDirs[i], center, scale);

        const float kDedupEpsSq = 1e-6f;
        Vector3 unique[14];
        int uniqueCount = 0;
        for (int i = 0; i < candCount; ++i)
        {
            const Vector3 c = candidates[i] - center;
            bool dup = false;
            for (int j = 0; j < uniqueCount; ++j)
            {
                const Vector3 d = c - unique[j];
                if (d.lengthSquared() < kDedupEpsSq)
                {
                    dup = true;
                    break;
                }
            }
            if (!dup && uniqueCount < 14)
                unique[uniqueCount++] = c;
        }

        const int out = (uniqueCount < maxVerts) ? uniqueCount : maxVerts;
        for (int i = 0; i < out; ++i)
            outVerts[i] = unique[i];
        *outCount = out;
    }
}

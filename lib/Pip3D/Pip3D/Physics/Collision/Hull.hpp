#pragma once

#include <cfloat>
#include <cmath>

#include "Math/Algebra.hpp"
#include "Geometry/Mesh.hpp"
#include "Physics/RigidBody/Body.hpp"

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

    inline bool computeConvexHullFaces(const Vector3 *verts, int vertCount,
                                       ConvexFace *outFaces, int *outFaceCount,
                                       int maxFaces) noexcept
    {
        *outFaceCount = 0;
        if (vertCount < 4 || maxFaces <= 0)
            return false;

        struct Plane
        {
            Vector3 normal;
            float offset;
        };
        Plane planes[64];
        int planeCount = 0;
        bool planeLimitReached = false;

        const float kCoplanarEps = 1e-5f;
        const float kNormalAlignEps = 0.999f;
        const float kOffsetEps = 1e-4f;

        for (int i = 0; i < vertCount; ++i)
        {
            for (int j = i + 1; j < vertCount; ++j)
            {
                for (int k = j + 1; k < vertCount; ++k)
                {
                    const Vector3 ab = verts[j] - verts[i];
                    const Vector3 ac = verts[k] - verts[i];
                    Vector3 n = ab.cross(ac);
                    const float nLenSq = n.lengthSquared();
                    if (nLenSq < 1e-10f)
                        continue;
                    n = n * FastMath::fastInvSqrt(nLenSq);
                    float d = n.dot(verts[i]);

                    int sign = 0;
                    bool isFace = true;
                    for (int m = 0; m < vertCount; ++m)
                    {
                        if (m == i || m == j || m == k)
                            continue;
                        const float sd = n.dot(verts[m]) - d;
                        if (fabsf(sd) < kCoplanarEps)
                            continue;
                        const int s = (sd > 0.0f) ? 1 : -1;
                        if (sign == 0)
                            sign = s;
                        else if (sign != s)
                        {
                            isFace = false;
                            break;
                        }
                    }
                    if (!isFace)
                        continue;

                    if (sign > 0)
                    {
                        n = -n;
                        d = -d;
                    }

                    bool found = false;
                    for (int p = 0; p < planeCount; ++p)
                    {
                        const float dotN = n.dot(planes[p].normal);
                        const float diffD = d - planes[p].offset;
                        if (dotN > kNormalAlignEps && fabsf(diffD) < kOffsetEps)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found && planeCount < 64)
                    {
                        planes[planeCount].normal = n;
                        planes[planeCount].offset = d;
                        ++planeCount;
                    }
                    else if (!found)
                    {
                        planeLimitReached = true;
                    }
                }
            }
        }

        bool complete = !planeLimitReached;
        for (int p = 0; p < planeCount; ++p)
        {
            if (*outFaceCount >= maxFaces)
            {
                complete = false;
                break;
            }
            const Vector3 &n = planes[p].normal;
            const float d = planes[p].offset;

            int onPlaneIdx[32];
            int onPlaneCount = 0;
            for (int v = 0; v < vertCount && onPlaneCount < 32; ++v)
            {
                if (fabsf(n.dot(verts[v]) - d) < kCoplanarEps)
                    onPlaneIdx[onPlaneCount++] = v;
            }
            if (onPlaneCount < 3)
                continue;
            if (onPlaneCount > 12)
            {
                complete = false;
                break;
            }

            Vector3 centroid(0, 0, 0);
            for (int v = 0; v < onPlaneCount; ++v)
                centroid += verts[onPlaneIdx[v]];
            centroid = centroid * (1.0f / float(onPlaneCount));

            Vector3 u;
            if (fabsf(n.x) < 0.9f)
                u = Vector3(1, 0, 0);
            else
                u = Vector3(0, 1, 0);
            u = u - n * u.dot(n);
            const float uLenSq = u.lengthSquared();
            if (uLenSq < 1e-12f)
                continue;
            u = u * FastMath::fastInvSqrt(uLenSq);
            const Vector3 w = n.cross(u);

            float angles[32];
            for (int v = 0; v < onPlaneCount; ++v)
            {
                const Vector3 r = verts[onPlaneIdx[v]] - centroid;
                const float cu = r.dot(u);
                const float cw = r.dot(w);
                angles[v] = atan2f(cw, cu);
            }

            int order[32];
            for (int v = 0; v < onPlaneCount; ++v)
                order[v] = v;
            for (int a = 1; a < onPlaneCount; ++a)
            {
                int key = order[a];
                int b = a - 1;
                while (b >= 0 && angles[order[b]] > angles[key])
                {
                    order[b + 1] = order[b];
                    --b;
                }
                order[b + 1] = key;
            }

            ConvexFace &face = outFaces[(*outFaceCount)++];
            face.normal = n;
            face.offset = d;
            const int keepCount = onPlaneCount;
            face.vertCount = static_cast<uint8_t>(keepCount);
            for (int v = 0; v < keepCount; ++v)
                face.vertIdx[v] = static_cast<uint8_t>(onPlaneIdx[order[v]]);
            for (int v = keepCount; v < 12; ++v)
                face.vertIdx[v] = 0;
        }
        return complete && *outFaceCount >= 4;
    }

    inline bool RigidBody::setConvex(const Vector3 *verts, int count) noexcept
    {
        if (!verts || count < 4 || count > kMaxConvexVerts)
            return false;

        Vector3 mn = verts[0];
        Vector3 mx = verts[0];
        Vector3 candidateVerts[kMaxConvexVerts];
        for (int i = 0; i < count; ++i)
        {
            candidateVerts[i] = verts[i];
            if (verts[i].x < mn.x)
                mn.x = verts[i].x;
            else if (verts[i].x > mx.x)
                mx.x = verts[i].x;
            if (verts[i].y < mn.y)
                mn.y = verts[i].y;
            else if (verts[i].y > mx.y)
                mx.y = verts[i].y;
            if (verts[i].z < mn.z)
                mn.z = verts[i].z;
            else if (verts[i].z > mx.z)
                mx.z = verts[i].z;
        }
        for (int i = count; i < kMaxConvexVerts; ++i)
            candidateVerts[i] = Vector3(0, 0, 0);

        ConvexFace candidateFaces[kMaxConvexFaces] = {};
        int candidateFaceCount = 0;
        if (!computeConvexHullFaces(candidateVerts, count,
                                    candidateFaces, &candidateFaceCount, kMaxConvexFaces))
            return false;

        shape = BODY_SHAPE_CONVEX;
        convexCount = count;
        convexFaceCount = candidateFaceCount;
        convexHullComplete = true;
        for (int i = 0; i < kMaxConvexVerts; ++i)
            convexVerts[i] = candidateVerts[i];
        for (int i = 0; i < kMaxConvexFaces; ++i)
            convexFaces[i] = candidateFaces[i];
        size = mx - mn;
        radius = 0.0f;
        for (int i = 0; i < convexCount; ++i)
        {
            const float r2 = convexVerts[i].lengthSquared();
            if (r2 > radius * radius)
                radius = sqrtf(r2);
        }
        capsuleHalfHeight = 0.0f;

        updateBoundsFromTransform();
        computeInertia();
        updateWorldInvInertia();
        return convexHullComplete;
    }

}

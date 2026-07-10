#pragma once

#include "Geometry/Mesh.hpp"

namespace pip3D
{
    class Sphere : public Mesh
    {
    private:
        struct SimpleEdge
        {
            uint16_t v0, v1;
        };

        struct EdgeSplitCache
        {
            SimpleEdge *edges;
            uint16_t   *midpoints;
            uint16_t    count;

            PIP3D_FORCE_INLINE uint16_t getOrCreate(uint16_t v0, uint16_t v1,
                                                    Vertex *verts, uint16_t &vCount)
            {
                if (v0 > v1)
                {
                    const uint16_t tmp = v0;
                    v0 = v1;
                    v1 = tmp;
                }

                for (uint16_t i = 0; i < count; ++i)
                    if (edges[i].v0 == v0 && edges[i].v1 == v1)
                        return midpoints[i];

                const float x  = static_cast<float>(verts[v0].px + verts[v1].px);
                const float y  = static_cast<float>(verts[v0].py + verts[v1].py);
                const float z  = static_cast<float>(verts[v0].pz + verts[v1].pz);
                const float lenSq = x * x + y * y + z * z;
                const float invLenScaled = 32767.0f * FastMath::fastInvSqrt(lenSq);

                const uint16_t newIdx = vCount++;
                const int16_t px = static_cast<int16_t>(x * invLenScaled);
                const int16_t py = static_cast<int16_t>(y * invLenScaled);
                const int16_t pz = static_cast<int16_t>(z * invLenScaled);

                verts[newIdx].px = px;
                verts[newIdx].py = py;
                verts[newIdx].pz = pz;

                constexpr float kInv32767 = 1.0f / 32767.0f;
                verts[newIdx].normal.set(static_cast<float>(px) * kInv32767,
                                          static_cast<float>(py) * kInv32767,
                                          static_cast<float>(pz) * kInv32767);

                edges[count]     = {v0, v1};
                midpoints[count] = newIdx;
                ++count;
                return newIdx;
            }
        };

        static constexpr uint16_t getIcosphereFaceCount(uint8_t subdivisions)
        {
            return 20u << (2 * subdivisions);
        }

        static constexpr uint16_t getIcosphereVertexCount(uint8_t subdivisions)
        {
            return (10u << (2 * subdivisions)) + 2u;
        }

        static PIP3D_FORCE_INLINE void computeUV(const Vertex &v, float &u, float &vv)
        {
            constexpr float invMaxVal = 1.0f / 32767.0f;
            const float x = static_cast<float>(v.px) * invMaxVal;
            const float y = static_cast<float>(v.py) * invMaxVal;
            const float z = static_cast<float>(v.pz) * invMaxVal;
            u  = 0.5f - atan2f(z, x) * (1.0f / kTwoPi);
            vv = 0.5f - asinf(clamp(y, -1.0f, 1.0f)) * (1.0f / kPi);
        }

    public:
        Sphere(float radius = 1.0f, uint8_t segments = 8, uint8_t rings = 6)
            : Mesh(getIcosphereVertexCount(segments <= 8 ? 1 : (segments <= 16 ? 2 : 3)) + 64,
                   getIcosphereFaceCount(segments <= 8 ? 1 : (segments <= 16 ? 2 : 3)))
        {
            (void)rings;

            autoScale(radius * 2.0f);
            if (unlikely(!vertices_ || !faces_))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Sphere: alloc failed");
                return;
            }

            const uint8_t subdivisions = segments <= 8 ? 1 : (segments <= 16 ? 2 : 3);

            constexpr float A = 0.525731112119f * 32767.0f;
            constexpr float B = 0.850650808352f * 32767.0f;

            const Vector3 baseVerts[12] = {
                {-A,  B, 0}, { A,  B, 0}, {-A, -B, 0}, { A, -B, 0},
                { 0, -A,  B}, { 0,  A,  B}, { 0, -A, -B}, { 0,  A, -B},
                { B, 0, -A}, { B, 0,  A}, {-B, 0, -A}, {-B, 0,  A}
            };

            uint16_t vCount = 0;
            for (int i = 0; i < 12; ++i)
            {
                Vertex &v = vertices_[vCount++];
                v.px = static_cast<int16_t>(baseVerts[i].x);
                v.py = static_cast<int16_t>(baseVerts[i].y);
                v.pz = static_cast<int16_t>(baseVerts[i].z);
                v.normal.set(baseVerts[i].x, baseVerts[i].y, baseVerts[i].z);
            }

            static constexpr Face baseFaces[20] = {
                {0, 11, 5},  {0, 5, 1},   {0, 1, 7},    {0, 7, 10},  {0, 10, 11},
                {1, 5, 9},   {5, 11, 4},  {11, 10, 2},  {10, 7, 6},  {7, 1, 8},
                {3, 9, 4},   {3, 4, 2},   {3, 2, 6},    {3, 6, 8},   {3, 8, 9},
                {4, 9, 5},   {2, 4, 11},  {6, 2, 10},   {8, 6, 7},   {9, 8, 1}
            };

            uint16_t fCount = 20;
            memcpy(faces_, baseFaces, 20 * sizeof(Face));

            for (uint8_t s = 1; s <= subdivisions; ++s)
            {
                const uint16_t oldFCount = fCount;

                Face *tempFaces = static_cast<Face *>(alloca(oldFCount * sizeof(Face)));
                memcpy(tempFaces, faces_, oldFCount * sizeof(Face));

                const uint16_t maxEdges = 30u << (2 * (s - 1));
                SimpleEdge *edges = static_cast<SimpleEdge *>(alloca(maxEdges * sizeof(SimpleEdge)));
                uint16_t   *midpoints = static_cast<uint16_t *>(alloca(maxEdges * sizeof(uint16_t)));

                EdgeSplitCache splitCache{edges, midpoints, 0};

                fCount = 0;
                for (uint16_t i = 0; i < oldFCount; ++i)
                {
                    const uint16_t v0 = tempFaces[i].v0;
                    const uint16_t v1 = tempFaces[i].v1;
                    const uint16_t v2 = tempFaces[i].v2;

                    const uint16_t a = splitCache.getOrCreate(v0, v1, vertices_, vCount);
                    const uint16_t b = splitCache.getOrCreate(v1, v2, vertices_, vCount);
                    const uint16_t c = splitCache.getOrCreate(v2, v0, vertices_, vCount);

                    faces_[fCount++] = Face(v0, a, c);
                    faces_[fCount++] = Face(v1, b, a);
                    faces_[fCount++] = Face(v2, c, b);
                    faces_[fCount++] = Face(a, b, c);
                }
            }

            for (uint16_t i = 0; i < vCount; ++i)
                computeUV(vertices_[i], vertices_[i].tu, vertices_[i].tv);

            const uint16_t originalVCount = vCount;
            uint16_t seamMap[256];
            memset(seamMap, 0, sizeof(seamMap));

            constexpr int16_t poleThreshold = static_cast<int16_t>(32767.0f * 0.998f);

            for (uint16_t i = 0; i < fCount; ++i)
            {
                Face &f = faces_[i];

                const float u0 = vertices_[f.v0].tu;
                const float u1 = vertices_[f.v1].tu;
                const float u2 = vertices_[f.v2].tu;

                if (fabsf(u0 - u1) > 0.5f || fabsf(u1 - u2) > 0.5f || fabsf(u2 - u0) > 0.5f)
                {
                    uint16_t *const idxPtrs[3] = {&f.v0, &f.v1, &f.v2};
                    for (int c = 0; c < 3; ++c)
                    {
                        uint16_t idx = *idxPtrs[c];
                        if (idx >= originalVCount) continue;
                        if (seamMap[idx] != 0)
                        {
                            *idxPtrs[c] = seamMap[idx];
                            continue;
                        }
                        if (vertices_[idx].tu >= 0.25f) continue;
                        if (vCount >= maxVertices_) continue;

                        const uint16_t newIdx = vCount++;
                        vertices_[newIdx] = vertices_[idx];
                        vertices_[newIdx].tu = vertices_[idx].tu + 1.0f;
                        seamMap[idx] = newIdx;
                        *idxPtrs[c] = newIdx;
                    }
                }

                const float cu0 = vertices_[f.v0].tu;
                const float cu1 = vertices_[f.v1].tu;
                const float cu2 = vertices_[f.v2].tu;

                int poleCorner = -1;
                if (vertices_[f.v0].py >=  poleThreshold || vertices_[f.v0].py <= -poleThreshold) poleCorner = 0;
                else if (vertices_[f.v1].py >=  poleThreshold || vertices_[f.v1].py <= -poleThreshold) poleCorner = 1;
                else if (vertices_[f.v2].py >=  poleThreshold || vertices_[f.v2].py <= -poleThreshold) poleCorner = 2;

                if (poleCorner >= 0 && vCount < maxVertices_)
                {
                    uint16_t poleIdx;
                    float avgU;
                    if (poleCorner == 0) { poleIdx = f.v0; avgU = (cu1 + cu2) * 0.5f; }
                    else if (poleCorner == 1) { poleIdx = f.v1; avgU = (cu0 + cu2) * 0.5f; }
                    else { poleIdx = f.v2; avgU = (cu0 + cu1) * 0.5f; }

                    const uint16_t newIdx = vCount++;
                    vertices_[newIdx] = vertices_[poleIdx];
                    vertices_[newIdx].tu = avgU;
                    if (poleCorner == 0) f.v0 = newIdx;
                    else if (poleCorner == 1) f.v1 = newIdx;
                    else f.v2 = newIdx;
                }
            }

            finalizeGeometry(vCount, fCount, Vector3(0.0f, 0.0f, 0.0f), radius);
            bindDeleter<Sphere>();
        }
    };

}
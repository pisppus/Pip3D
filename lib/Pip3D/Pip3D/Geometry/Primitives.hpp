#pragma once

#include "Mesh.hpp"

namespace pip3D
{
    namespace detail
    {
        static constexpr float constAbs(float v)
        {
            return v < 0.0f ? -v : v;
        }

        static constexpr uint16_t packNormalConstexpr(float x, float y, float z)
        {
            float l1norm = constAbs(x) + constAbs(y) + constAbs(z);
            if (l1norm > 1e-6f)
            {
                float inv_norm = 1.0f / l1norm;
                float nx = x * inv_norm;
                float ny = y * inv_norm;

                if (z < 0.0f)
                {
                    float tx = nx;
                    nx = (1.0f - constAbs(ny)) * (nx >= 0.0f ? 1.0f : -1.0f);
                    ny = (1.0f - constAbs(tx)) * (ny >= 0.0f ? 1.0f : -1.0f);
                }

                float px_f = (nx * 0.5f + 0.5f) * 255.0f;
                float py_f = (ny * 0.5f + 0.5f) * 255.0f;

                uint32_t px = static_cast<uint32_t>(px_f);
                uint32_t py = static_cast<uint32_t>(py_f);

                return static_cast<uint16_t>((px << 8) | py);
            }
            return 0;
        }

        alignas(16) inline const Vertex s_cubeVertices[24] = {
            {-32767, -32767, -32767, packNormalConstexpr(0.0f, 0.0f, -1.0f), 0.0f, 0.0f},
            {32767, -32767, -32767, packNormalConstexpr(0.0f, 0.0f, -1.0f), 1.0f, 0.0f},
            {32767, 32767, -32767, packNormalConstexpr(0.0f, 0.0f, -1.0f), 1.0f, 1.0f},
            {-32767, 32767, -32767, packNormalConstexpr(0.0f, 0.0f, -1.0f), 0.0f, 1.0f},

            {32767, -32767, 32767, packNormalConstexpr(0.0f, 0.0f, 1.0f), 0.0f, 0.0f},
            {-32767, -32767, 32767, packNormalConstexpr(0.0f, 0.0f, 1.0f), 1.0f, 0.0f},
            {-32767, 32767, 32767, packNormalConstexpr(0.0f, 0.0f, 1.0f), 1.0f, 1.0f},
            {32767, 32767, 32767, packNormalConstexpr(0.0f, 0.0f, 1.0f), 0.0f, 1.0f},

            {-32767, 32767, -32767, packNormalConstexpr(0.0f, 1.0f, 0.0f), 0.0f, 0.0f},
            {32767, 32767, -32767, packNormalConstexpr(0.0f, 1.0f, 0.0f), 1.0f, 0.0f},
            {32767, 32767, 32767, packNormalConstexpr(0.0f, 1.0f, 0.0f), 1.0f, 1.0f},
            {-32767, 32767, 32767, packNormalConstexpr(0.0f, 1.0f, 0.0f), 0.0f, 1.0f},

            {-32767, -32767, 32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 0.0f, 0.0f},
            {32767, -32767, 32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 1.0f, 0.0f},
            {32767, -32767, -32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 1.0f, 1.0f},
            {-32767, -32767, -32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 0.0f, 1.0f},

            {32767, -32767, -32767, packNormalConstexpr(1.0f, 0.0f, 0.0f), 0.0f, 0.0f},
            {32767, -32767, 32767, packNormalConstexpr(1.0f, 0.0f, 0.0f), 1.0f, 0.0f},
            {32767, 32767, 32767, packNormalConstexpr(1.0f, 0.0f, 0.0f), 1.0f, 1.0f},
            {32767, 32767, -32767, packNormalConstexpr(1.0f, 0.0f, 0.0f), 0.0f, 1.0f},

            {-32767, -32767, 32767, packNormalConstexpr(-1.0f, 0.0f, 0.0f), 0.0f, 0.0f},
            {-32767, -32767, -32767, packNormalConstexpr(-1.0f, 0.0f, 0.0f), 1.0f, 0.0f},
            {-32767, 32767, -32767, packNormalConstexpr(-1.0f, 0.0f, 0.0f), 1.0f, 1.0f},
            {-32767, 32767, 32767, packNormalConstexpr(-1.0f, 0.0f, 0.0f), 0.0f, 1.0f}};

        inline const Face s_cubeFaces[12] = {
            {0, 2, 1},
            {0, 3, 2},
            {4, 6, 5},
            {4, 7, 6},
            {8, 10, 9},
            {8, 11, 10},
            {12, 14, 13},
            {12, 15, 14},
            {16, 18, 17},
            {16, 19, 18},
            {20, 22, 21},
            {20, 23, 22}};

        alignas(16) inline const Vertex s_pyramidVertices[16] = {
            {0, 32767, 0, packNormalConstexpr(0.0f, 1.0f, -2.0f), 0.5f, 1.0f},
            {-32767, -32767, -32767, packNormalConstexpr(0.0f, 1.0f, -2.0f), 0.0f, 0.0f},
            {32767, -32767, -32767, packNormalConstexpr(0.0f, 1.0f, -2.0f), 1.0f, 0.0f},

            {0, 32767, 0, packNormalConstexpr(2.0f, 1.0f, 0.0f), 0.5f, 1.0f},
            {32767, -32767, -32767, packNormalConstexpr(2.0f, 1.0f, 0.0f), 0.0f, 0.0f},
            {32767, -32767, 32767, packNormalConstexpr(2.0f, 1.0f, 0.0f), 1.0f, 0.0f},

            {0, 32767, 0, packNormalConstexpr(0.0f, 1.0f, 2.0f), 0.5f, 1.0f},
            {32767, -32767, 32767, packNormalConstexpr(0.0f, 1.0f, 2.0f), 0.0f, 0.0f},
            {-32767, -32767, 32767, packNormalConstexpr(0.0f, 1.0f, 2.0f), 1.0f, 0.0f},

            {0, 32767, 0, packNormalConstexpr(-2.0f, 1.0f, 0.0f), 0.5f, 1.0f},
            {-32767, -32767, 32767, packNormalConstexpr(-2.0f, 1.0f, 0.0f), 0.0f, 0.0f},
            {-32767, -32767, -32767, packNormalConstexpr(-2.0f, 1.0f, 0.0f), 1.0f, 0.0f},

            {-32767, -32767, -32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 0.0f, 0.0f},
            {32767, -32767, -32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 1.0f, 0.0f},
            {32767, -32767, 32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 1.0f, 1.0f},
            {-32767, -32767, 32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 0.0f, 1.0f}};

        inline const Face s_pyramidFaces[6] = {
            {0, 2, 1},
            {3, 5, 4},
            {6, 8, 7},
            {9, 11, 10},
            {12, 13, 14},
            {12, 14, 15}};
    }

    class Cube : public Mesh
    {
    public:
        Cube(float size = 1.0f, const Color &color = Color::WHITE)
            : Mesh(detail::s_cubeVertices, 24, detail::s_cubeFaces, 12, color, true)
        {
            autoScale(size);
            finalizeGeometry(24, 12, Vector3(0.0f, 0.0f, 0.0f), size * 0.8660254f);
        }
    };

    class Pyramid : public Mesh
    {
    public:
        Pyramid(float size = 1.0f, const Color &color = Color::WHITE)
            : Mesh(detail::s_pyramidVertices, 16, detail::s_pyramidFaces, 6, color, true)
        {
            autoScale(size);
            finalizeGeometry(16, 6, Vector3(0.0f, -size * 0.25f, 0.0f), size * 0.75f);
        }
    };

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
            uint16_t *midpoints;
            uint16_t count;

            __attribute__((always_inline)) inline uint16_t getOrCreate(uint16_t v0, uint16_t v1, Vertex *verts, uint16_t &vCount)
            {
                if (v0 > v1)
                {
                    std::swap(v0, v1);
                }

                for (uint16_t i = 0; i < count; ++i)
                {
                    if (edges[i].v0 == v0 && edges[i].v1 == v1)
                    {
                        return midpoints[i];
                    }
                }

                float x = static_cast<float>(verts[v0].px + verts[v1].px);
                float y = static_cast<float>(verts[v0].py + verts[v1].py);
                float z = static_cast<float>(verts[v0].pz + verts[v1].pz);

                float lenSq = x * x + y * y + z * z;
                float invLen = (1.0f / sqrtf(lenSq)) * 32767.0f;

                uint16_t newIdx = vCount++;
                int16_t px = static_cast<int16_t>(x * invLen);
                int16_t py = static_cast<int16_t>(y * invLen);
                int16_t pz = static_cast<int16_t>(z * invLen);

                verts[newIdx].px = px;
                verts[newIdx].py = py;
                verts[newIdx].pz = pz;

                float fx = static_cast<float>(px);
                float fy = static_cast<float>(py);
                float fz = static_cast<float>(pz);

                float l1norm = fabsf(fx) + fabsf(fy) + fabsf(fz);
                float inv_l1 = FastMath::fastReciprocal(l1norm);
                float nx = fx * inv_l1;
                float ny = fy * inv_l1;

                if (pz < 0)
                {
                    float tx = nx;
                    nx = (1.0f - fabsf(ny)) * (nx >= 0.0f ? 1.0f : -1.0f);
                    ny = (1.0f - fabsf(tx)) * (ny >= 0.0f ? 1.0f : -1.0f);
                }
                uint32_t npx = static_cast<uint32_t>((nx * 0.5f + 0.5f) * 255.0f);
                uint32_t npy = static_cast<uint32_t>((ny * 0.5f + 0.5f) * 255.0f);
                verts[newIdx].normal.data = (npx << 8) | npy;

                edges[count] = {v0, v1};
                midpoints[count] = newIdx;
                count++;
                return newIdx;
            }
        };

        static constexpr uint16_t getIcosphereFaceCount(uint8_t subdivisions)
        {
            return 20 << (2 * subdivisions);
        }

        static constexpr uint16_t getIcosphereVertexCount(uint8_t subdivisions)
        {
            uint16_t baseCount = (10 << (2 * subdivisions)) + 2;
            return baseCount + (28 << subdivisions);
        }

    public:
        Sphere(float radius = 1.0f, uint8_t segments = 8, uint8_t rings = 6, const Color &color = Color::WHITE)
            : Mesh(getIcosphereVertexCount(segments <= 8 ? 1 : (segments <= 16 ? 2 : 3)),
                   getIcosphereFaceCount(segments <= 8 ? 1 : (segments <= 16 ? 2 : 3)),
                   color)
        {
            autoScale(radius * 2.0f);

            if (unlikely(!vertices || !faces))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Sphere: Mesh base allocation failed");
                return;
            }

            const uint8_t subdivisions = segments <= 8 ? 1 : (segments <= 16 ? 2 : 3);

            const float A = 0.525731112119f * 32767.0f;
            const float B = 0.850650808352f * 32767.0f;

            Vector3 baseVerts[12] = {
                {-A, B, 0},
                {A, B, 0},
                {-A, -B, 0},
                {A, -B, 0},
                {0, -A, B},
                {0, A, B},
                {0, -A, -B},
                {0, A, -B},
                {B, 0, -A},
                {B, 0, A},
                {-B, 0, -A},
                {-B, 0, A}};

            uint16_t vCount = 0;
            for (int i = 0; i < 12; ++i)
            {
                Vertex &v = vertices[vCount++];
                v.px = static_cast<int16_t>(baseVerts[i].x);
                v.py = static_cast<int16_t>(baseVerts[i].y);
                v.pz = static_cast<int16_t>(baseVerts[i].z);

                Vector3 norm = baseVerts[i];
                norm.normalize();

                float l1norm = fabsf(norm.x) + fabsf(norm.y) + fabsf(norm.z);
                float inv_norm = FastMath::fastReciprocal(l1norm);
                float nx = norm.x * inv_norm;
                float ny = norm.y * inv_norm;
                if (norm.z < 0.0f)
                {
                    float tx = nx;
                    nx = (1.0f - fabsf(ny)) * (nx >= 0.0f ? 1.0f : -1.0f);
                    ny = (1.0f - fabsf(tx)) * (ny >= 0.0f ? 1.0f : -1.0f);
                }
                uint32_t px = (uint32_t)((nx * 0.5f + 0.5f) * 255.0f);
                uint32_t py = (uint32_t)((ny * 0.5f + 0.5f) * 255.0f);
                v.normal.data = (px << 8) | py;
            }

            static const Face baseFaces[20] = {
                {0, 11, 5},
                {0, 5, 1},
                {0, 1, 7},
                {0, 7, 10},
                {0, 10, 11},
                {1, 5, 9},
                {5, 11, 4},
                {11, 10, 2},
                {10, 7, 6},
                {7, 1, 8},
                {3, 9, 4},
                {3, 4, 2},
                {3, 2, 6},
                {3, 6, 8},
                {3, 8, 9},
                {4, 9, 5},
                {2, 4, 11},
                {6, 2, 10},
                {8, 6, 7},
                {9, 8, 1}};

            uint16_t fCount = 20;
            memcpy(faces, baseFaces, 20 * sizeof(Face));

            for (uint8_t s = 1; s <= subdivisions; ++s)
            {
                const uint16_t oldFCount = fCount;

                Face *tempFaces = (Face *)alloca(oldFCount * sizeof(Face));
                memcpy(tempFaces, faces, oldFCount * sizeof(Face));

                const uint16_t maxEdges = 30 << (2 * (s - 1));
                SimpleEdge *edges = (SimpleEdge *)alloca(maxEdges * sizeof(SimpleEdge));
                uint16_t *midpoints = (uint16_t *)alloca(maxEdges * sizeof(uint16_t));

                EdgeSplitCache splitCache{edges, midpoints, 0};

                fCount = 0;
                for (uint16_t i = 0; i < oldFCount; ++i)
                {
                    uint16_t v0 = tempFaces[i].v0;
                    uint16_t v1 = tempFaces[i].v1;
                    uint16_t v2 = tempFaces[i].v2;

                    uint16_t a = splitCache.getOrCreate(v0, v1, vertices, vCount);
                    uint16_t b = splitCache.getOrCreate(v1, v2, vertices, vCount);
                    uint16_t c = splitCache.getOrCreate(v2, v0, vertices, vCount);

                    faces[fCount++] = Face(v0, a, c);
                    faces[fCount++] = Face(v1, b, a);
                    faces[fCount++] = Face(v2, c, b);
                    faces[fCount++] = Face(a, b, c);
                }
            }

            const float invMaxVal = 1.0f / 32767.0f;
            const float invTwoPi = 1.0f / kTwoPi;
            const float invPi = 1.0f / kPi;

            for (uint16_t i = 0; i < vCount; ++i)
            {
                float x = static_cast<float>(vertices[i].px) * invMaxVal;
                float y = static_cast<float>(vertices[i].py) * invMaxVal;
                float z = static_cast<float>(vertices[i].pz) * invMaxVal;

                vertices[i].tu = 0.5f - atan2f(z, x) * invTwoPi;
                vertices[i].tv = 0.5f - asinf(clamp(y, -1.0f, 1.0f)) * invPi;
            }

            const uint16_t originalVCount = vCount;
            uint16_t seamMap[1024];
            if (originalVCount < 1024)
            {
                memset(seamMap, 0, originalVCount * sizeof(uint16_t));
            }
            else
            {
                return;
            }

            for (uint16_t i = 0; i < fCount; ++i)
            {
                uint16_t A = faces[i].v0;
                uint16_t B = faces[i].v1;
                uint16_t C = faces[i].v2;

                float u0 = vertices[A].tu;
                float u1 = vertices[B].tu;
                float u2 = vertices[C].tu;

                if (fabsf(u0 - u1) > 0.5f || fabsf(u1 - u2) > 0.5f || fabsf(u2 - u0) > 0.5f)
                {
                    auto duplicateSeamVertex = [&](uint16_t &index)
                    {
                        if (index >= originalVCount)
                            return;

                        float uVal = vertices[index].tu;
                        if (uVal < 0.25f)
                        {
                            if (seamMap[index] != 0)
                            {
                                index = seamMap[index];
                            }
                            else if (vCount < maxVertices)
                            {
                                uint16_t newIdx = vCount++;
                                vertices[newIdx] = vertices[index];
                                vertices[newIdx].tu = uVal + 1.0f;
                                seamMap[index] = newIdx;
                                index = newIdx;
                            }
                        }
                    };

                    duplicateSeamVertex(faces[i].v0);
                    duplicateSeamVertex(faces[i].v1);
                    duplicateSeamVertex(faces[i].v2);
                }
            }

            auto isPoleVertex = [&](uint16_t index) -> bool
            {
                float y = static_cast<float>(vertices[index].py) * invMaxVal;
                return fabsf(y) > 0.998f;
            };

            for (uint16_t i = 0; i < fCount; ++i)
            {
                uint16_t A = faces[i].v0;
                uint16_t B = faces[i].v1;
                uint16_t C = faces[i].v2;

                if (isPoleVertex(A))
                {
                    if (vCount < maxVertices)
                    {
                        uint16_t newIdx = vCount++;
                        vertices[newIdx] = vertices[A];
                        vertices[newIdx].tu = (vertices[B].tu + vertices[C].tu) * 0.5f;
                        faces[i].v0 = newIdx;
                    }
                }
                if (isPoleVertex(B))
                {
                    if (vCount < maxVertices)
                    {
                        uint16_t newIdx = vCount++;
                        vertices[newIdx] = vertices[B];
                        vertices[newIdx].tu = (vertices[A].tu + vertices[C].tu) * 0.5f;
                        faces[i].v1 = newIdx;
                    }
                }
                if (isPoleVertex(C))
                {
                    if (vCount < maxVertices)
                    {
                        uint16_t newIdx = vCount++;
                        vertices[newIdx] = vertices[C];
                        vertices[newIdx].tu = (vertices[A].tu + vertices[B].tu) * 0.5f;
                        faces[i].v2 = newIdx;
                    }
                }
            }

            finalizeGeometry(vCount, fCount, Vector3(0.0f, 0.0f, 0.0f), radius);
        }

        Sphere(float radius, const Color &color)
            : Sphere(radius, 16, 12, color)
        {
        }
    };

    class Plane : public Mesh
    {
    public:
        Plane(float width = 2.0f, float depth = 2.0f, uint8_t subdivisions = 1, const Color &color = Color::WHITE, float uvScale = 1.0f)
            : Mesh(((subdivisions ? subdivisions : 1) + 1) * ((subdivisions ? subdivisions : 1) + 1),
                   (subdivisions ? subdivisions : 1) * (subdivisions ? subdivisions : 1) * 2, color)
        {
            setSingleColorLighting(true);
            const float size = (width > depth) ? width : depth;
            autoScale(size);

            if (unlikely(!vertices || !faces))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Plane: Mesh base allocation failed");
                return;
            }

            const uint8_t divs = subdivisions ? subdivisions : 1;
            const float ratioX = width / size;
            const float ratioZ = depth / size;

            const float qStartX = -ratioX * 32767.0f;
            const float qEndX = ratioX * 32767.0f;
            const float qStartZ = -ratioZ * 32767.0f;
            const float qEndZ = ratioZ * 32767.0f;

            const float stepX = (qEndX - qStartX) / divs;
            const float stepZ = (qEndZ - qStartZ) / divs;

            Vertex *__restrict vPtr = vertices;
            const uint16_t normalUp = detail::packNormalConstexpr(0.0f, 1.0f, 0.0f);

            const float invDivs = 1.0f / static_cast<float>(divs);
            const float scaleU = invDivs * uvScale;
            const float scaleV = invDivs * uvScale;

            for (uint8_t z = 0; z <= divs; z++)
            {
                float currentZ = qStartZ + static_cast<float>(z) * stepZ;
                const int16_t qZ = (z == divs) ? static_cast<int16_t>(lrintf(qEndZ)) : static_cast<int16_t>(lrintf(currentZ));
                const float tv_val = static_cast<float>(z) * scaleV;

                for (uint8_t x = 0; x <= divs; x++)
                {
                    float currentX = qStartX + static_cast<float>(x) * stepX;
                    const int16_t qX = (x == divs) ? static_cast<int16_t>(lrintf(qEndX)) : static_cast<int16_t>(lrintf(currentX));

                    vPtr->px = qX;
                    vPtr->py = 0;
                    vPtr->pz = qZ;
                    vPtr->normal.data = normalUp;

                    vPtr->tu = static_cast<float>(x) * scaleU;
                    vPtr->tv = tv_val;

                    vPtr++;
                }
            }

            Face *__restrict fPtr = faces;
            const uint16_t pitch = divs + 1;
            uint16_t i0 = 0;
            uint16_t i1 = pitch;

            for (uint8_t z = 0; z < divs; z++)
            {
                for (uint8_t x = 0; x < divs; x++)
                {
                    fPtr[0] = Face(i0, i1, i0 + 1);
                    fPtr[1] = Face(i0 + 1, i1, i1 + 1);
                    fPtr += 2;

                    i0++;
                    i1++;
                }
                i0++;
                i1++;
            }

            finalizeGeometry(static_cast<uint16_t>(vPtr - vertices),
                             static_cast<uint16_t>(fPtr - faces),
                             Vector3(0.0f, 0.0f, 0.0f),
                             0.5f * sqrtf(width * width + depth * depth));
        }
    };

    class Cylinder : public Mesh
    {
    public:
        Cylinder(float radius = 1.0f, float height = 2.0f, uint8_t segments = 16, const Color &color = Color::WHITE, float uvScaleU = 1.0f, float uvScaleV = 1.0f)
            : Mesh(4 + (segments ? segments : 3) * 4, (segments ? segments : 3) * 4, color)
        {
            const float size = (height > radius * 2.0f) ? height : radius * 2.0f;
            autoScale(size);

            if (unlikely(!vertices || !faces))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Cylinder: Mesh base allocation failed");
                return;
            }

            const uint8_t segs = segments ? segments : 3;
            const float h = height * 0.5f;

            const float halfSize = size * 0.5f;
            const float invHalfSize = FastMath::fastReciprocal(halfSize);

            const float scaleR = (radius * invHalfSize) * 32767.0f;
            const float scaleH = (h * invHalfSize) * 32767.0f;

            const int16_t qH = static_cast<int16_t>(lrintf(scaleH));
            uint16_t vIdx = 0;

            const uint16_t angleBinStep = 65536 / segs;
            const float invSegs = 1.0f / static_cast<float>(segs);
            const float scaleU = invSegs * uvScaleU;

            uint16_t sideTopStart = vIdx;
            for (uint8_t i = 0; i <= segs; i++)
            {
                uint16_t angleBin = (i == segs) ? 0 : i * angleBinStep;
                float s, c;
                FastMath::fastSinCosBin(angleBin, s, c);
                int16_t qRx = static_cast<int16_t>(lrintf(c * scaleR));
                int16_t qRz = static_cast<int16_t>(lrintf(s * scaleR));
                float u = static_cast<float>(i) * scaleU;

                vertices[vIdx++] = Vertex(qRx, qH, qRz, detail::packNormalConstexpr(c, 0.0f, s), u, 0.0f);
            }

            uint16_t sideBottomStart = vIdx;
            for (uint8_t i = 0; i <= segs; i++)
            {
                uint16_t angleBin = (i == segs) ? 0 : i * angleBinStep;
                float s, c;
                FastMath::fastSinCosBin(angleBin, s, c);
                int16_t qRx = static_cast<int16_t>(lrintf(c * scaleR));
                int16_t qRz = static_cast<int16_t>(lrintf(s * scaleR));
                float u = static_cast<float>(i) * scaleU;

                vertices[vIdx++] = Vertex(qRx, -qH, qRz, detail::packNormalConstexpr(c, 0.0f, s), u, uvScaleV);
            }

            uint16_t topCenterIdx = vIdx;
            vertices[vIdx++] = Vertex(0, qH, 0, detail::packNormalConstexpr(0.0f, 1.0f, 0.0f), 0.5f, 0.5f);

            uint16_t topCapStart = vIdx;
            for (uint8_t i = 0; i < segs; i++)
            {
                uint16_t angleBin = i * angleBinStep;
                float s, c;
                FastMath::fastSinCosBin(angleBin, s, c);
                int16_t qRx = static_cast<int16_t>(lrintf(c * scaleR));
                int16_t qRz = static_cast<int16_t>(lrintf(s * scaleR));

                float u = 0.5f + 0.5f * c;
                float v = 0.5f + 0.5f * s;
                vertices[vIdx++] = Vertex(qRx, qH, qRz, detail::packNormalConstexpr(0.0f, 1.0f, 0.0f), u, v);
            }

            uint16_t bottomCenterIdx = vIdx;
            vertices[vIdx++] = Vertex(0, -qH, 0, detail::packNormalConstexpr(0.0f, -1.0f, 0.0f), 0.5f, 0.5f);

            uint16_t bottomCapStart = vIdx;
            for (uint8_t i = 0; i < segs; i++)
            {
                uint16_t angleBin = i * angleBinStep;
                float s, c;
                FastMath::fastSinCosBin(angleBin, s, c);
                int16_t qRx = static_cast<int16_t>(lrintf(c * scaleR));
                int16_t qRz = static_cast<int16_t>(lrintf(s * scaleR));

                float u = 0.5f + 0.5f * c;
                float v = 0.5f + 0.5f * s;
                vertices[vIdx++] = Vertex(qRx, -qH, qRz, detail::packNormalConstexpr(0.0f, -1.0f, 0.0f), u, v);
            }

            uint16_t fIdx = 0;
            for (uint8_t i = 0; i < segs; i++)
            {
                uint16_t next = i + 1;

                uint16_t t1 = sideTopStart + i;
                uint16_t t2 = sideTopStart + next;
                uint16_t b1 = sideBottomStart + i;
                uint16_t b2 = sideBottomStart + next;

                faces[fIdx++] = Face(t1, t2, b1);
                faces[fIdx++] = Face(t2, b2, b1);

                uint16_t capNext = (i + 1 == segs) ? 0 : i + 1;

                uint16_t tc1 = topCapStart + i;
                uint16_t tc2 = topCapStart + capNext;
                faces[fIdx++] = Face(topCenterIdx, tc2, tc1);

                uint16_t bc1 = bottomCapStart + i;
                uint16_t bc2 = bottomCapStart + capNext;
                faces[fIdx++] = Face(bottomCenterIdx, bc1, bc2);
            }

            finalizeGeometry(vIdx, fIdx, Vector3(0.0f, 0.0f, 0.0f), sqrtf(radius * radius + h * h));
        }
    };

    class Cone : public Mesh
    {
    public:
        Cone(float radius = 1.0f, float height = 2.0f, uint8_t segments = 16, const Color &color = Color::WHITE, float uvScaleU = 1.0f, float uvScaleV = 1.0f)
            : Mesh(3 + (segments ? segments : 3) * 3, (segments ? segments : 3) * 2, color)
        {
            const float size = (height > radius * 2.0f) ? height : radius * 2.0f;
            autoScale(size);

            if (unlikely(!vertices || !faces))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "Cone: Mesh base allocation failed");
                return;
            }

            const uint8_t segs = segments ? segments : 3;
            const float h = height * 0.5f;

            const float halfSize = size * 0.5f;
            const float invHalfSize = FastMath::fastReciprocal(halfSize);

            const float scaleR = (radius * invHalfSize) * 32767.0f;
            const float scaleH = (h * invHalfSize) * 32767.0f;

            const int16_t qH = static_cast<int16_t>(lrintf(scaleH));

            Vertex *__restrict vPtr = vertices;

            const uint16_t angleBinStep = 65536 / segs;
            const float invSegs = 1.0f / static_cast<float>(segs);

            uint16_t sideApexStart = (uint16_t)(vPtr - vertices);
            for (uint8_t i = 0; i <= segs; i++)
            {
                uint16_t angleBin = (i == segs) ? 0 : i * angleBinStep;
                float sinAngle, cosAngle;
                FastMath::fastSinCosBin(angleBin, sinAngle, cosAngle);

                const float nx_raw = height * cosAngle;
                const float ny_raw = -radius;
                const float nz_raw = height * sinAngle;

                const float l1norm = fabsf(nx_raw) + fabsf(ny_raw) + fabsf(nz_raw);
                const float inv_l1 = FastMath::fastReciprocal(l1norm);

                float nx = nx_raw * inv_l1;
                float ny = ny_raw * inv_l1;
                const float nz = nz_raw * inv_l1;

                float nx_folded = (1.0f - fabsf(ny)) * __builtin_copysignf(1.0f, nx);
                float ny_folded = (1.0f - fabsf(nx)) * __builtin_copysignf(1.0f, ny);

                nx = (nz < 0.0f) ? nx_folded : nx;
                ny = (nz < 0.0f) ? ny_folded : ny;

                uint32_t px = static_cast<uint32_t>((nx * 0.5f + 0.5f) * 255.0f);
                uint32_t py = static_cast<uint32_t>((ny * 0.5f + 0.5f) * 255.0f);

                float u = static_cast<float>(i) * invSegs * uvScaleU;

                vPtr->px = 0;
                vPtr->py = qH;
                vPtr->pz = 0;
                vPtr->normal.data = (px << 8) | py;
                vPtr->tu = u;
                vPtr->tv = 0.0f;
                vPtr++;
            }

            uint16_t sideBaseStart = (uint16_t)(vPtr - vertices);
            for (uint8_t i = 0; i <= segs; i++)
            {
                uint16_t angleBin = (i == segs) ? 0 : i * angleBinStep;
                float sinAngle, cosAngle;
                FastMath::fastSinCosBin(angleBin, sinAngle, cosAngle);

                const int16_t qRx = static_cast<int16_t>(lrintf(cosAngle * scaleR));
                const int16_t qRz = static_cast<int16_t>(lrintf(sinAngle * scaleR));

                const float nx_raw = height * cosAngle;
                const float ny_raw = -radius;
                const float nz_raw = height * sinAngle;

                const float l1norm = fabsf(nx_raw) + fabsf(ny_raw) + fabsf(nz_raw);
                const float inv_l1 = FastMath::fastReciprocal(l1norm);

                float nx = nx_raw * inv_l1;
                float ny = ny_raw * inv_l1;
                const float nz = nz_raw * inv_l1;

                float nx_folded = (1.0f - fabsf(ny)) * __builtin_copysignf(1.0f, nx);
                float ny_folded = (1.0f - fabsf(nx)) * __builtin_copysignf(1.0f, ny);

                nx = (nz < 0.0f) ? nx_folded : nx;
                ny = (nz < 0.0f) ? ny_folded : ny;

                uint32_t px = static_cast<uint32_t>((nx * 0.5f + 0.5f) * 255.0f);
                uint32_t py = static_cast<uint32_t>((ny * 0.5f + 0.5f) * 255.0f);

                float u = static_cast<float>(i) * invSegs * uvScaleU;

                vPtr->px = qRx;
                vPtr->py = -qH;
                vPtr->pz = qRz;
                vPtr->normal.data = (px << 8) | py;
                vPtr->tu = u;
                vPtr->tv = uvScaleV;
                vPtr++;
            }

            uint16_t baseCenterIdx = (uint16_t)(vPtr - vertices);
            {
                vPtr->px = 0;
                vPtr->py = -qH;
                vPtr->pz = 0;
                vPtr->normal.data = detail::packNormalConstexpr(0.0f, -1.0f, 0.0f);
                vPtr->tu = 0.5f;
                vPtr->tv = 0.5f;
                vPtr++;
            }

            uint16_t bottomCapStart = (uint16_t)(vPtr - vertices);
            for (uint8_t i = 0; i < segs; i++)
            {
                uint16_t angleBin = i * angleBinStep;
                float sinAngle, cosAngle;
                FastMath::fastSinCosBin(angleBin, sinAngle, cosAngle);

                const int16_t qRx = static_cast<int16_t>(lrintf(cosAngle * scaleR));
                const int16_t qRz = static_cast<int16_t>(lrintf(sinAngle * scaleR));

                vPtr->px = qRx;
                vPtr->py = -qH;
                vPtr->pz = qRz;
                vPtr->normal.data = detail::packNormalConstexpr(0.0f, -1.0f, 0.0f);
                vPtr->tu = 0.5f + 0.5f * cosAngle;
                vPtr->tv = 0.5f + 0.5f * sinAngle;
                vPtr++;
            }

            Face *__restrict fPtr = faces;
            for (uint8_t i = 0; i < segs; i++)
            {
                uint16_t next = i + 1;

                uint16_t apex = sideApexStart + i;
                uint16_t b1 = sideBaseStart + i;
                uint16_t b2 = sideBaseStart + next;

                fPtr[0] = Face(apex, b2, b1);

                uint16_t capNext = (i + 1 == segs) ? 0 : i + 1;
                uint16_t bc1 = bottomCapStart + i;
                uint16_t bc2 = bottomCapStart + capNext;
                fPtr[1] = Face(baseCenterIdx, bc1, bc2);

                fPtr += 2;
            }

            float yc = 0.0f;
            float r_bound = 0.0f;
            if (radius > height)
            {
                yc = -h;
                r_bound = radius;
            }
            else
            {
                yc = -(radius * radius) / (2.0f * height);
                r_bound = h - yc;
            }

            finalizeGeometry(static_cast<uint16_t>(vPtr - vertices),
                             static_cast<uint16_t>(segs * 2),
                             Vector3(0.0f, yc, 0.0f),
                             r_bound);
        }
    };

    static constexpr uint16_t getCapsuleVertexCount(uint8_t segments, uint8_t rings, float height, float radius)
    {
        uint8_t segs = segments ? segments : 3;
        uint8_t hemiRings = rings ? rings : 1;
        bool hasCylinder = (height - 2.0f * radius) > 0.0001f;
        uint16_t baseCount = 2 + segs * (((hemiRings > 1 ? hemiRings - 1 : 0) * 2) + (hasCylinder ? 2 : 1));
        return baseCount + segs * 2 + 32;
    }

    class Capsule : public Mesh
    {
    private:
        __attribute__((always_inline)) static inline void packUnitNormalBranchless(
            PackedNormal &normal, float nx, float ny, float nz)
        {
            float l1norm = fabsf(nx) + fabsf(ny) + fabsf(nz);
            float inv_l1 = FastMath::fastReciprocal(l1norm);
            float ox = nx * inv_l1;
            float oy = ny * inv_l1;

            float ox_folded = (1.0f - fabsf(oy)) * __builtin_copysignf(1.0f, ox);
            float oy_folded = (1.0f - fabsf(ox)) * __builtin_copysignf(1.0f, oy);

            ox = (nz < 0.0f) ? ox_folded : ox;
            oy = (nz < 0.0f) ? oy_folded : oy;

            uint32_t px = static_cast<uint32_t>((ox * 0.5f + 0.5f) * 255.0f);
            uint32_t py = static_cast<uint32_t>((oy * 0.5f + 0.5f) * 255.0f);
            normal.data = (px << 8) | py;
        }

    public:
        Capsule(float radius = 1.0f, float height = 2.0f, uint8_t segments = 12, uint8_t rings = 6, const Color &color = Color::WHITE)
            : Mesh(getCapsuleVertexCount(segments, rings, height, radius),
                   (segments ? segments : 3) * ((((rings ? rings : 1) > 1 ? ((rings ? rings : 1) - 1) : 0) * 2 + (((height - 2.0f * radius) > 0.0001f) ? 2 : 1) - 1) * 2 + 2),
                   color)
        {
            const float size = (height > radius * 2.0f) ? height : radius * 2.0f;
            autoScale(size);

            if (unlikely(!vertices || !faces))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Capsule: Mesh base allocation failed");
                return;
            }

            constexpr float two = 2.0f;
            constexpr float half = 0.5f;
            const float cylinderHeight = fmaxf(0.0f, height - two * radius);
            const float halfCyl = cylinderHeight * half;
            const uint8_t segs = segments ? segments : 3;
            const uint8_t hemiRings = rings ? rings : 1;
            const bool hasCylinder = cylinderHeight > 0.0001f;

            const float halfSize = size * 0.5f;
            const float invHalfSize = FastMath::fastReciprocal(halfSize);

            const float scaleR = (radius * invHalfSize) * 32767.0f;
            const float scaleCyl = (halfCyl * invHalfSize) * 32767.0f;

            Vertex *__restrict vPtr = vertices;
            uint16_t ringRows = 0;

            const uint16_t topPoleIdx = 0;
            {
                vPtr->px = 0;
                vPtr->py = static_cast<int16_t>(lrintf(scaleCyl + scaleR));
                vPtr->pz = 0;
                vPtr->normal.data = detail::packNormalConstexpr(0.0f, 1.0f, 0.0f);
                vPtr++;
            }

            float localSinCache[64];
            float localCosCache[64];
            float *__restrict sinThetaCache = nullptr;
            float *__restrict cosThetaCache = nullptr;
            const bool useStack = (segs <= 64);

            if (likely(useStack))
            {
                sinThetaCache = localSinCache;
                cosThetaCache = localCosCache;
            }
            else
            {
                sinThetaCache = (float *)MemUtils::allocData(segs * sizeof(float), 16);
                cosThetaCache = (float *)MemUtils::allocData(segs * sizeof(float), 16);
            }

            const float invSegs = 1.0f / static_cast<float>(segs);
            for (uint8_t j = 0; j < segs; j++)
            {
                const float theta = kTwoPi * static_cast<float>(j) * invSegs;
                float sinAngle, cosAngle;
                FastMath::fastSinCos(theta, sinAngle, cosAngle);

                sinThetaCache[j] = sinAngle;
                cosThetaCache[j] = cosAngle;
            }

            const float invHemiRings = 1.0f / static_cast<float>(hemiRings);
            const float phi_step = (kPi * 0.5f) * invHemiRings;

            for (uint8_t ring = 1; ring < hemiRings; ++ring)
            {
                const float phi = static_cast<float>(ring) * phi_step;
                float sinPhi, cosPhi;
                FastMath::fastSinCos(phi, sinPhi, cosPhi);

                const int16_t qY = static_cast<int16_t>(lrintf(scaleCyl + scaleR * cosPhi));
                const float r_scale = scaleR * sinPhi;

                for (uint8_t seg = 0; seg < segs; ++seg)
                {
                    const float sinTheta = sinThetaCache[seg];
                    const float cosTheta = cosThetaCache[seg];

                    vPtr->px = static_cast<int16_t>(lrintf(r_scale * cosTheta));
                    vPtr->py = qY;
                    vPtr->pz = static_cast<int16_t>(lrintf(r_scale * sinTheta));

                    packUnitNormalBranchless(vPtr->normal, sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
                    vPtr++;
                }
                ++ringRows;
            }

            const int16_t qTopSeamY = static_cast<int16_t>(lrintf(scaleCyl));
            {
                for (uint8_t seg = 0; seg < segs; ++seg)
                {
                    const float sinTheta = sinThetaCache[seg];
                    const float cosTheta = cosThetaCache[seg];

                    vPtr->px = static_cast<int16_t>(lrintf(scaleR * cosTheta));
                    vPtr->py = qTopSeamY;
                    vPtr->pz = static_cast<int16_t>(lrintf(scaleR * sinTheta));

                    packUnitNormalBranchless(vPtr->normal, cosTheta, 0.0f, sinTheta);
                    vPtr++;
                }
                ++ringRows;
            }

            if (hasCylinder)
            {
                const int16_t qBottomSeamY = static_cast<int16_t>(lrintf(-scaleCyl));
                for (uint8_t seg = 0; seg < segs; ++seg)
                {
                    const float sinTheta = sinThetaCache[seg];
                    const float cosTheta = cosThetaCache[seg];

                    vPtr->px = static_cast<int16_t>(lrintf(scaleR * cosTheta));
                    vPtr->py = qBottomSeamY;
                    vPtr->pz = static_cast<int16_t>(lrintf(scaleR * sinTheta));

                    packUnitNormalBranchless(vPtr->normal, cosTheta, 0.0f, sinTheta);
                    vPtr++;
                }
                ++ringRows;
            }

            for (int ring = static_cast<int>(hemiRings) - 1; ring >= 1; --ring)
            {
                const float phi = static_cast<float>(ring) * phi_step;
                float sinPhi, cosPhi;
                FastMath::fastSinCos(phi, sinPhi, cosPhi);

                const int16_t qY = static_cast<int16_t>(lrintf(-scaleCyl - scaleR * cosPhi));
                const float r_scale = scaleR * sinPhi;

                for (uint8_t seg = 0; seg < segs; ++seg)
                {
                    const float sinTheta = sinThetaCache[seg];
                    const float cosTheta = cosThetaCache[seg];

                    vPtr->px = static_cast<int16_t>(lrintf(r_scale * cosTheta));
                    vPtr->py = qY;
                    vPtr->pz = static_cast<int16_t>(lrintf(r_scale * sinTheta));

                    packUnitNormalBranchless(vPtr->normal, sinPhi * cosTheta, -cosPhi, sinPhi * sinTheta);
                    vPtr++;
                }
                ++ringRows;
            }

            const uint16_t bottomPoleIdx = static_cast<uint16_t>(vPtr - vertices);
            {
                vPtr->px = 0;
                vPtr->py = static_cast<int16_t>(lrintf(-scaleCyl - scaleR));
                vPtr->pz = 0;
                vPtr->normal.data = detail::packNormalConstexpr(0.0f, -1.0f, 0.0f);
                vPtr++;
            }

            if (unlikely(!useStack))
            {
                MemUtils::freeData(sinThetaCache);
                MemUtils::freeData(cosThetaCache);
            }

            uint16_t vCount = static_cast<uint16_t>(vPtr - vertices);

            const float invTwoPi = 1.0f / kTwoPi;
            const float total_L = kPi * scaleR + (hasCylinder ? (2.0f * scaleCyl) : 0.0f);

            for (uint16_t i = 0; i < vCount; ++i)
            {
                float px_val = static_cast<float>(vertices[i].px);
                float py_val = static_cast<float>(vertices[i].py);
                float pz_val = static_cast<float>(vertices[i].pz);

                vertices[i].tu = 0.5f - atan2f(pz_val, px_val) * invTwoPi;

                if (py_val >= scaleCyl)
                {
                    float dy = py_val - scaleCyl;
                    float phi = acosf(clamp(dy / scaleR, -1.0f, 1.0f));
                    vertices[i].tv = (phi * scaleR) / total_L;
                }
                else if (py_val <= -scaleCyl)
                {
                    float dy = -scaleCyl - py_val;
                    float phi = acosf(clamp(dy / scaleR, -1.0f, 1.0f));
                    float d = (kPi * 0.5f) * scaleR + (hasCylinder ? (2.0f * scaleCyl) : 0.0f) + (kPi * 0.5f - phi) * scaleR;
                    vertices[i].tv = d / total_L;
                }
                else
                {
                    float d = (kPi * 0.5f) * scaleR + (scaleCyl - py_val);
                    vertices[i].tv = d / total_L;
                }
            }

            Face *__restrict fPtr = faces;
            const uint16_t firstRingStart = 1;

            for (uint8_t seg = 0; seg < segs - 1; ++seg)
            {
                *fPtr++ = Face(topPoleIdx, firstRingStart + seg + 1, firstRingStart + seg);
            }
            *fPtr++ = Face(topPoleIdx, firstRingStart, firstRingStart + segs - 1);

            for (uint16_t ring = 0; ring < ringRows - 1; ++ring)
            {
                const uint16_t currRow = 1 + ring * segs;
                const uint16_t nextRow = currRow + segs;

                for (uint8_t seg = 0; seg < segs - 1; ++seg)
                {
                    const uint16_t curr = currRow + seg;
                    const uint16_t currNext = curr + 1;
                    const uint16_t below = nextRow + seg;
                    const uint16_t belowNext = below + 1;

                    fPtr[0] = Face(curr, currNext, below);
                    fPtr[1] = Face(currNext, belowNext, below);
                    fPtr += 2;
                }

                {
                    const uint16_t curr = currRow + segs - 1;
                    const uint16_t currNext = currRow;
                    const uint16_t below = nextRow + segs - 1;
                    const uint16_t belowNext = nextRow;

                    fPtr[0] = Face(curr, currNext, below);
                    fPtr[1] = Face(currNext, belowNext, below);
                    fPtr += 2;
                }
            }

            const uint16_t lastRingStart = 1 + (ringRows - 1) * segs;
            for (uint8_t seg = 0; seg < segs - 1; ++seg)
            {
                *fPtr++ = Face(bottomPoleIdx, lastRingStart + seg, lastRingStart + seg + 1);
            }
            *fPtr++ = Face(bottomPoleIdx, lastRingStart + segs - 1, lastRingStart);

            uint16_t fCount = static_cast<uint16_t>(fPtr - faces);

            const uint16_t originalVCount = vCount;
            uint16_t seamMap[1024];
            if (originalVCount < 1024)
            {
                memset(seamMap, 0, originalVCount * sizeof(uint16_t));
            }
            else
            {
                return;
            }

            for (uint16_t i = 0; i < fCount; ++i)
            {
                uint16_t A = faces[i].v0;
                uint16_t B = faces[i].v1;
                uint16_t C = faces[i].v2;

                float u0 = vertices[A].tu;
                float u1 = vertices[B].tu;
                float u2 = vertices[C].tu;

                if (fabsf(u0 - u1) > 0.5f || fabsf(u1 - u2) > 0.5f || fabsf(u2 - u0) > 0.5f)
                {
                    auto duplicateSeamVertex = [&](uint16_t &index)
                    {
                        if (index >= originalVCount)
                            return;

                        float uVal = vertices[index].tu;
                        if (uVal < 0.25f)
                        {
                            if (seamMap[index] != 0)
                            {
                                index = seamMap[index];
                            }
                            else if (vCount < maxVertices)
                            {
                                uint16_t newIdx = vCount++;
                                vertices[newIdx] = vertices[index];
                                vertices[newIdx].tu = uVal + 1.0f;
                                seamMap[index] = newIdx;
                                index = newIdx;
                            }
                        }
                    };

                    duplicateSeamVertex(faces[i].v0);
                    duplicateSeamVertex(faces[i].v1);
                    duplicateSeamVertex(faces[i].v2);
                }
            }

            auto isPoleVertex = [&](uint16_t index) -> bool
            {
                float limit = scaleCyl + scaleR - 5.0f;
                return fabsf(static_cast<float>(vertices[index].py)) > limit;
            };

            for (uint16_t i = 0; i < fCount; ++i)
            {
                uint16_t A = faces[i].v0;
                uint16_t B = faces[i].v1;
                uint16_t C = faces[i].v2;

                if (isPoleVertex(A))
                {
                    if (vCount < maxVertices)
                    {
                        uint16_t newIdx = vCount++;
                        vertices[newIdx] = vertices[A];
                        vertices[newIdx].tu = (vertices[B].tu + vertices[C].tu) * 0.5f;
                        faces[i].v0 = newIdx;
                    }
                }
                if (isPoleVertex(B))
                {
                    if (vCount < maxVertices)
                    {
                        uint16_t newIdx = vCount++;
                        vertices[newIdx] = vertices[B];
                        vertices[newIdx].tu = (vertices[A].tu + vertices[C].tu) * 0.5f;
                        faces[i].v1 = newIdx;
                    }
                }
                if (isPoleVertex(C))
                {
                    if (vCount < maxVertices)
                    {
                        uint16_t newIdx = vCount++;
                        vertices[newIdx] = vertices[C];
                        vertices[newIdx].tu = (vertices[A].tu + vertices[B].tu) * 0.5f;
                        faces[i].v2 = newIdx;
                    }
                }
            }

            finalizeGeometry(vCount, fCount, Vector3(0.0f, 0.0f, 0.0f), height * 0.5f);
        }
    };

    class TrefoilKnot : public Mesh
    {
    private:
        __attribute__((always_inline)) static inline void packUnitNormalBranchless(
            PackedNormal &normal, float nx, float ny, float nz)
        {
            const float l1 = fabsf(nx) + fabsf(ny) + fabsf(nz);
            const float inv = FastMath::fastReciprocal(l1);
            float ox = nx * inv;
            float oy = ny * inv;

            if (nz < 0.0f)
            {
                const float sgnX = __builtin_copysignf(1.0f, ox);
                const float sgnY = __builtin_copysignf(1.0f, oy);
                const float ox_f = (1.0f - fabsf(oy)) * sgnX;
                const float oy_f = (1.0f - fabsf(ox)) * sgnY;
                ox = ox_f;
                oy = oy_f;
            }

            const uint32_t px = (uint32_t)(int32_t)(ox * 127.5f + 127.5f);
            const uint32_t py = (uint32_t)(int32_t)(oy * 127.5f + 127.5f);
            normal.data = (px << 8) | py;
        }

        __attribute__((always_inline)) static inline Vector3 transportN(
            const Vector3 &N_prev, const Vector3 &T_prev,
            const Vector3 &T_curr, const Vector3 &path_delta)
        {
            const float c1 = path_delta.dot(path_delta);
            if (c1 < 1e-8f)
                return N_prev;

            const float inv_c1 = FastMath::fastReciprocal(c1);
            const float k1 = 2.0f * path_delta.dot(N_prev) * inv_c1;
            const float k1t = 2.0f * path_delta.dot(T_prev) * inv_c1;
            const Vector3 NL = N_prev - path_delta * k1;
            const Vector3 TL = T_prev - path_delta * k1t;

            const Vector3 v2 = T_curr - TL;
            const float c2 = v2.dot(v2);
            if (c2 < 1e-8f)
                return NL;

            return NL - v2 * (2.0f * v2.dot(NL) * FastMath::fastReciprocal(c2));
        }

    public:
        TrefoilKnot(float scale = 1.0f,
                    uint8_t segments = 64,
                    uint8_t tubeSegments = 12,
                    const Color &color = Color::WHITE,
                    float uvScaleU = 1.0f,
                    float uvScaleV = 1.0f)
            : Mesh(((segments ? segments : 3) + 1) * ((tubeSegments ? tubeSegments : 3) + 1),
                   (segments ? segments : 3) * (tubeSegments ? tubeSegments : 3) * 2,
                   color)
        {
            autoScale(scale * 7.5f);

            if (!vertices || !faces)
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "TrefoilKnot: base alloc failed (v=%p f=%p)",
                     (void *)vertices, (void *)faces);
                return;
            }

            const uint8_t segs = segments ? segments : 3;
            const uint8_t tubeSegs = tubeSegments ? tubeSegments : 3;

            constexpr float tubeScale = 0.55f;
            const float tubeRadius = tubeScale * scale;

            alignas(16) float localCos[64];
            alignas(16) float localSin[64];
            float *__restrict cosC = nullptr;
            float *__restrict sinC = nullptr;
            const bool useStack = (tubeSegs <= 64);

            if (likely(useStack))
            {
                cosC = localCos;
                sinC = localSin;
            }
            else
            {
                cosC = (float *)MemUtils::allocData(tubeSegs * sizeof(float), 16);
                sinC = (float *)MemUtils::allocData(tubeSegs * sizeof(float), 16);
                if (unlikely(!cosC || !sinC))
                {
                    if (cosC)
                        MemUtils::freeData(cosC);
                    if (sinC)
                        MemUtils::freeData(sinC);
                    return;
                }
            }

            const float invTS = 1.0f / (float)tubeSegs;
            for (uint8_t j = 0; j < tubeSegs; ++j)
            {
                const float a = kTwoPi * (float)j * invTS;
                FastMath::fastSinCos(a, sinC[j], cosC[j]);
            }

            Vector3 *__restrict path = (Vector3 *)MemUtils::allocData((segs + 1) * sizeof(Vector3), 16);
            if (unlikely(!path))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "TrefoilKnot: path alloc failed");
                if (!useStack)
                {
                    MemUtils::freeData(cosC);
                    MemUtils::freeData(sinC);
                }
                return;
            }

            const float invS = 1.0f / (float)segs;
            for (uint8_t i = 0; i < segs; ++i)
            {
                const float t = kTwoPi * (float)i * invS;
                float s2, c2, s3, c3;
                FastMath::fastSinCos(2.0f * t, s2, c2);
                FastMath::fastSinCos(3.0f * t, s3, c3);

                const float r = scale * (2.0f + c3);
                path[i].x = r * c2;
                path[i].y = r * s2;
                path[i].z = scale * (-s3 * 1.4f);
            }
            path[segs] = path[0];

            Vector3 *__restrict T = (Vector3 *)MemUtils::allocData((segs + 1) * sizeof(Vector3), 16);
            Vector3 *__restrict N = (Vector3 *)MemUtils::allocData((segs + 1) * sizeof(Vector3), 16);
            Vector3 *__restrict B = (Vector3 *)MemUtils::allocData((segs + 1) * sizeof(Vector3), 16);

            if (unlikely(!T || !N || !B))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "TrefoilKnot: frame alloc failed");
                if (T)
                    MemUtils::freeData(T);
                if (N)
                    MemUtils::freeData(N);
                if (B)
                    MemUtils::freeData(B);
                MemUtils::freeData(path);
                if (!useStack)
                {
                    MemUtils::freeData(cosC);
                    MemUtils::freeData(sinC);
                }
                return;
            }

            T[0] = path[1] - path[0];
            T[0].normalize();
            Vector3 U(fabsf(T[0].y) > 0.9f ? Vector3(1, 0, 0) : Vector3(0, 1, 0));
            N[0] = T[0].cross(U);
            N[0].normalize();
            B[0] = T[0].cross(N[0]);

            for (uint8_t i = 1; i < segs; ++i)
            {
                const uint8_t next = i + 1;
                T[i] = path[next] - path[i];
                T[i].normalize();

                const Vector3 delta = path[i] - path[i - 1];
                N[i] = transportN(N[i - 1], T[i - 1], T[i], delta);
                N[i].normalize();
                B[i] = T[i].cross(N[i]);
            }

            {
                const Vector3 delta_last = path[0] - path[segs - 1];
                Vector3 N_transported = transportN(N[segs - 1], T[segs - 1], T[0], delta_last);
                N_transported.normalize();

                float cosA = N_transported.dot(N[0]);
                cosA = clamp(cosA, -1.0f, 1.0f);
                float angle_diff = acosf(cosA);

                const Vector3 cr = N_transported.cross(N[0]);
                if (cr.dot(T[0]) < 0.0f)
                {
                    angle_diff = -angle_diff;
                }

                const float invSeg = 1.0f / (float)segs;
                for (uint8_t i = 1; i < segs; ++i)
                {
                    const float angle = (float)i * angle_diff * invSeg;
                    float s, c;
                    FastMath::fastSinCos(angle, s, c);

                    const Vector3 oN = N[i];
                    const Vector3 oB = B[i];
                    N[i] = oN * c + oB * s;
                    B[i] = oB * c - oN * s;
                }
            }

            T[segs] = T[0];
            N[segs] = N[0];
            B[segs] = B[0];

            Vertex *__restrict vPtr = vertices;
            const float invQ = qScale > 1e-6f ? (1.0f / qScale) : 1.0f;

            for (uint8_t i = 0; i <= segs; ++i)
            {
                const float px_c = path[i].x;
                const float py_c = path[i].y;
                const float pz_c = path[i].z;

                const float Nx = N[i].x, Ny = N[i].y, Nz = N[i].z;
                const float Bx = B[i].x, By = B[i].y, Bz = B[i].z;

                const float u = static_cast<float>(i) * invS * uvScaleU;

                for (uint8_t j = 0; j <= tubeSegs; ++j)
                {
                    uint8_t indexJ = (j == tubeSegs) ? 0 : j;
                    const float cn = cosC[indexJ];
                    const float sn = sinC[indexJ];

                    const float dx = cn * Nx + sn * Bx;
                    const float dy = cn * Ny + sn * By;
                    const float dz = cn * Nz + sn * Bz;

                    const float vx = px_c + tubeRadius * dx;
                    const float vy = py_c + tubeRadius * dy;
                    const float vz = pz_c + tubeRadius * dz;

                    const float qx = vx * invQ;
                    const float qy = vy * invQ;
                    const float qz = vz * invQ;

                    vPtr->px = static_cast<int16_t>(fminf(fmaxf(qx, -32768.0f), 32767.0f));
                    vPtr->py = static_cast<int16_t>(fminf(fmaxf(qy, -32768.0f), 32767.0f));
                    vPtr->pz = static_cast<int16_t>(fminf(fmaxf(qz, -32768.0f), 32767.0f));

                    packUnitNormalBranchless(vPtr->normal, dx, dy, dz);

                    vPtr->tu = u;
                    vPtr->tv = static_cast<float>(j) * invTS * uvScaleV;
                    ++vPtr;
                }
            }

            MemUtils::freeData(T);
            MemUtils::freeData(N);
            MemUtils::freeData(B);
            MemUtils::freeData(path);
            if (unlikely(!useStack))
            {
                MemUtils::freeData(cosC);
                MemUtils::freeData(sinC);
            }

            vertexCount = (uint16_t)(vPtr - vertices);
            Face *__restrict fPtr = faces;
            const uint16_t pitch = tubeSegs + 1;

            for (uint8_t i = 0; i < segs; ++i)
            {
                const uint16_t rowCurrent = i * pitch;
                const uint16_t rowNext = (i + 1) * pitch;

                for (uint8_t j = 0; j < tubeSegs; ++j)
                {
                    const uint16_t a = rowCurrent + j;
                    const uint16_t b = rowNext + j;
                    const uint16_t c = rowNext + j + 1;
                    const uint16_t d = rowCurrent + j + 1;

                    fPtr[0] = Face(a, c, b);
                    fPtr[1] = Face(a, d, c);
                    fPtr += 2;
                }
            }

            vertexCount = (uint16_t)(vPtr - vertices);
            faceCount = (uint16_t)(fPtr - faces);
            finalize();
        }

    private:
        inline void finalize()
        {
            finalizeNormals();
            calculateBoundingSphere();
            finalizeTransform();
        }
    };
}
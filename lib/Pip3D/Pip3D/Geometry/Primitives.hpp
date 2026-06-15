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

        alignas(16) inline const Vertex s_cubeVertices[8] = {
            {-32767, -32767, -32767, packNormalConstexpr(-1.0f, -1.0f, -1.0f)},
            {32767, -32767, -32767, packNormalConstexpr(1.0f, -1.0f, -1.0f)},
            {32767, 32767, -32767, packNormalConstexpr(1.0f, 1.0f, -1.0f)},
            {-32767, 32767, -32767, packNormalConstexpr(-1.0f, 1.0f, -1.0f)},
            {-32767, -32767, 32767, packNormalConstexpr(-1.0f, -1.0f, 1.0f)},
            {32767, -32767, 32767, packNormalConstexpr(1.0f, -1.0f, 1.0f)},
            {32767, 32767, 32767, packNormalConstexpr(1.0f, 1.0f, 1.0f)},
            {-32767, 32767, 32767, packNormalConstexpr(-1.0f, 1.0f, 1.0f)}};

        inline const Face s_cubeFaces[12] = {
            {0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {4, 3, 0}, {4, 7, 3}, {1, 2, 6}, {1, 6, 5}, {3, 7, 6}, {3, 6, 2}, {4, 0, 1}, {4, 1, 5}};

        alignas(16) inline const Vertex s_pyramidVertices[5] = {
            {0, 32767, 0, packNormalConstexpr(0.0f, 1.0f, 0.0f)},
            {-32767, -32767, -32767, packNormalConstexpr(-1.0f, -1.0f, -1.0f)},
            {32767, -32767, -32767, packNormalConstexpr(1.0f, -1.0f, -1.0f)},
            {32767, -32767, 32767, packNormalConstexpr(1.0f, -1.0f, 1.0f)},
            {-32767, -32767, 32767, packNormalConstexpr(-1.0f, -1.0f, 1.0f)}};

        inline const Face s_pyramidFaces[6] = {
            {1, 2, 3}, {1, 3, 4}, {0, 2, 1}, {0, 3, 2}, {0, 4, 3}, {0, 1, 4}};
    }

    class Cube : public Mesh
    {
    public:
        Cube(float size = 1.0f, const Color &color = Color::WHITE)
            : Mesh(detail::s_cubeVertices, 8, detail::s_cubeFaces, 12, color, true)
        {
            autoScale(size);
            cache.boundingCenter = Vector3(0.0f, 0.0f, 0.0f);
            cache.boundingRadius = size * 0.8660254f;
            cache.boundsValid = true;

            cache.transform.identity();
            cache.maxScale = 1.0f;
            cache.transformValid = true;
            transformDirty = false;
            cache.transformHash = 4216742517u;
        }
    };

    class Pyramid : public Mesh
    {
    public:
        Pyramid(float size = 1.0f, const Color &color = Color::WHITE)
            : Mesh(detail::s_pyramidVertices, 5, detail::s_pyramidFaces, 6, color, true)
        {
            autoScale(size);
            cache.boundingCenter = Vector3(0.0f, -size * 0.25f, 0.0f);
            cache.boundingRadius = size * 0.75f;
            cache.boundsValid = true;

            cache.transform.identity();
            cache.maxScale = 1.0f;
            cache.transformValid = true;
            transformDirty = false;
            cache.transformHash = 4216742517u;
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
                float invLen = FastMath::fastInvSqrt(lenSq) * 32767.0f;

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
            return (10 << (2 * subdivisions)) + 2;
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
                {-A, B, 0}, {A, B, 0}, {-A, -B, 0}, {A, -B, 0}, {0, -A, B}, {0, A, B}, {0, -A, -B}, {0, A, -B}, {B, 0, -A}, {B, 0, A}, {-B, 0, -A}, {-B, 0, A}};

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
                {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8}, {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};

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

            vertexCount = vCount;
            faceCount = fCount;

            cache.boundingCenter = Vector3(0.0f, 0.0f, 0.0f);
            cache.boundingRadius = radius;
            cache.boundsValid = true;

            cache.transform.identity();
            cache.maxScale = 1.0f;
            cache.transformValid = true;
            transformDirty = false;
            cache.transformHash = 4216742517u;
        }

        Sphere(float radius, const Color &color)
            : Sphere(radius, 16, 12, color)
        {
        }
    };

    class Plane : public Mesh
    {
    public:
        Plane(float width = 2.0f, float depth = 2.0f, uint8_t subdivisions = 1, const Color &color = Color::WHITE)
            : Mesh(((subdivisions ? subdivisions : 1) + 1) * ((subdivisions ? subdivisions : 1) + 1),
                   (subdivisions ? subdivisions : 1) * (subdivisions ? subdivisions : 1) * 2,
                   color)
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

            const int16_t qStartX = static_cast<int16_t>(-ratioX * 32767.0f - 0.5f);
            const int16_t qEndX = static_cast<int16_t>(ratioX * 32767.0f + 0.5f);
            const int16_t qStartZ = static_cast<int16_t>(-ratioZ * 32767.0f - 0.5f);
            const int16_t qEndZ = static_cast<int16_t>(ratioZ * 32767.0f + 0.5f);

            const int32_t qZ_step = (static_cast<int32_t>(qEndZ - qStartZ) << 16) / divs;
            const int32_t qX_step = (static_cast<int32_t>(qEndX - qStartX) << 16) / divs;

            Vertex *__restrict vPtr = vertices;
            const uint16_t normalUp = detail::packNormalConstexpr(0.0f, 1.0f, 0.0f);

            int32_t qZ_fixed = qStartZ << 16;
            const int32_t qX_fixed_start = qStartX << 16;

            for (uint8_t z = 0; z <= divs; z++)
            {
                const int16_t qZ = (z == divs) ? qEndZ : static_cast<int16_t>(qZ_fixed >> 16);
                int32_t qX_fixed = qX_fixed_start;

                for (uint8_t x = 0; x <= divs; x++)
                {
                    const int16_t qX = (x == divs) ? qEndX : static_cast<int16_t>(qX_fixed >> 16);

                    vPtr->px = qX;
                    vPtr->py = 0;
                    vPtr->pz = qZ;
                    vPtr->normal.data = normalUp;
                    vPtr++;

                    qX_fixed += qX_step;
                }
                qZ_fixed += qZ_step;
            }
            vertexCount = static_cast<uint16_t>(vPtr - vertices);

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
            faceCount = static_cast<uint16_t>(fPtr - faces);

            cache.boundingCenter = Vector3(0.0f, 0.0f, 0.0f);
            cache.boundingRadius = 0.5f * sqrtf(width * width + depth * depth);
            cache.boundsValid = true;

            cache.transform.identity();
            cache.maxScale = 1.0f;
            cache.transformValid = true;
            transformDirty = false;
            cache.transformHash = 4216742517u;
        }
    };

    class Cylinder : public Mesh
    {
    public:
        Cylinder(float radius = 1.0f, float height = 2.0f, uint8_t segments = 16, const Color &color = Color::WHITE)
            : Mesh(2 + (segments ? segments : 3) * 2,
                   (segments ? segments : 3) * 4,
                   color)
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

            const int16_t qH = static_cast<int16_t>(scaleH + 0.5f);

            Vertex *__restrict topRingPtr = vertices + 1;
            Vertex *__restrict bottomRingPtr = vertices + 1 + segs;

            {
                vertices[0].px = 0;
                vertices[0].py = qH;
                vertices[0].pz = 0;
                vertices[0].normal.data = detail::packNormalConstexpr(0.0f, 1.0f, 0.0f);
            }

            const float invSegs = 1.0f / static_cast<float>(segs);
            for (uint8_t i = 0; i < segs; i++)
            {
                const float angle = kTwoPi * static_cast<float>(i) * invSegs;
                float sinAngle, cosAngle;
                FastMath::fastSinCos(angle, sinAngle, cosAngle);

                const int16_t qRx = static_cast<int16_t>(cosAngle * scaleR + (cosAngle >= 0.0f ? 0.5f : -0.5f));
                const int16_t qRz = static_cast<int16_t>(sinAngle * scaleR + (sinAngle >= 0.0f ? 0.5f : -0.5f));

                topRingPtr->px = qRx;
                topRingPtr->py = qH;
                topRingPtr->pz = qRz;

                bottomRingPtr->px = qRx;
                bottomRingPtr->py = -qH;
                bottomRingPtr->pz = qRz;

                const float nx_raw = height * cosAngle;
                const float ny_raw = radius;
                const float nz_raw = height * sinAngle;

                const float l1norm = fabsf(nx_raw) + fabsf(ny_raw) + fabsf(nz_raw);
                const float inv_l1 = FastMath::fastReciprocal(l1norm);

                const float nx = nx_raw * inv_l1;
                const float ny = ny_raw * inv_l1;
                const float nz = nz_raw * inv_l1;

                float nx_top = nx;
                float ny_top = ny;
                if (nz < 0.0f)
                {
                    float tx = nx_top;
                    nx_top = (1.0f - fabsf(ny_top)) * (nx_top >= 0.0f ? 1.0f : -1.0f);
                    ny_top = (1.0f - fabsf(tx)) * (ny_top >= 0.0f ? 1.0f : -1.0f);
                }
                uint32_t px_top = static_cast<uint32_t>((nx_top * 0.5f + 0.5f) * 255.0f);
                uint32_t py_top = static_cast<uint32_t>((ny_top * 0.5f + 0.5f) * 255.0f);
                topRingPtr->normal.data = (px_top << 8) | py_top;

                float nx_bot = nx;
                float ny_bot = -ny;
                if (nz < 0.0f)
                {
                    float tx = nx_bot;
                    nx_bot = (1.0f - fabsf(ny_bot)) * (nx_bot >= 0.0f ? 1.0f : -1.0f);
                    ny_bot = (1.0f - fabsf(tx)) * (ny_bot >= 0.0f ? 1.0f : -1.0f);
                }
                uint32_t px_bot = static_cast<uint32_t>((nx_bot * 0.5f + 0.5f) * 255.0f);
                uint32_t py_bot = static_cast<uint32_t>((ny_bot * 0.5f + 0.5f) * 255.0f);
                bottomRingPtr->normal.data = (px_bot << 8) | py_bot;

                topRingPtr++;
                bottomRingPtr++;
            }

            const uint16_t bottomCenter = 1 + segs * 2;
            {
                vertices[bottomCenter].px = 0;
                vertices[bottomCenter].py = static_cast<int16_t>(-scaleH + 0.5f);
                vertices[bottomCenter].pz = 0;
                vertices[bottomCenter].normal.data = detail::packNormalConstexpr(0.0f, -1.0f, 0.0f);
            }

            vertexCount = 2 + segs * 2;

            Face *__restrict fPtr = faces;
            const uint16_t topCenter = 0;

            for (uint8_t i = 0; i < segs - 1; i++)
            {
                const uint16_t top1 = 1 + i;
                const uint16_t top2 = 2 + i;
                const uint16_t bot1 = 1 + segs + i;
                const uint16_t bot2 = 2 + segs + i;

                fPtr[0] = Face(topCenter, top2, top1);
                fPtr[1] = Face(bottomCenter, bot1, bot2);
                fPtr[2] = Face(top1, top2, bot1);
                fPtr[3] = Face(top2, bot2, bot1);
                fPtr += 4;
            }

            {
                const uint16_t i = segs - 1;
                const uint16_t top1 = 1 + i;
                const uint16_t top2 = 1;
                const uint16_t bot1 = 1 + segs + i;
                const uint16_t bot2 = 1 + segs;

                fPtr[0] = Face(topCenter, top2, top1);
                fPtr[1] = Face(bottomCenter, bot1, bot2);
                fPtr[2] = Face(top1, top2, bot1);
                fPtr[3] = Face(top2, bot2, bot1);
            }
            faceCount = segs * 4;

            cache.boundingCenter = Vector3(0.0f, 0.0f, 0.0f);
            cache.boundingRadius = sqrtf(radius * radius + h * h);
            cache.boundsValid = true;

            cache.transform.identity();
            cache.maxScale = 1.0f;
            cache.transformValid = true;
            transformDirty = false;
            cache.transformHash = 4216742517u;
        }
    };

    class Cone : public Mesh
    {
    public:
        Cone(float radius = 1.0f, float height = 2.0f, uint8_t segments = 16, const Color &color = Color::WHITE)
            : Mesh((segments ? segments : 3) + 2,
                   (segments ? segments : 3) * 2,
                   color)
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

            {
                vPtr->px = 0;
                vPtr->py = qH;
                vPtr->pz = 0;
                vPtr->normal.data = detail::packNormalConstexpr(0.0f, 1.0f, 0.0f);
                vPtr++;
            }

            const float binAngleStep = 65536.0f / static_cast<float>(segs);
            float binAngleF = 0.0f;

            for (uint8_t i = 0; i < segs; i++)
            {
                uint16_t binAngle = static_cast<uint16_t>(binAngleF);
                float sinAngle, cosAngle;
                FastMath::fastSinCosBin(binAngle, sinAngle, cosAngle);

                const int16_t qRx = static_cast<int16_t>(lrintf(cosAngle * scaleR));
                const int16_t qRz = static_cast<int16_t>(lrintf(sinAngle * scaleR));

                vPtr->px = qRx;
                vPtr->py = -qH;
                vPtr->pz = qRz;

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
                vPtr->normal.data = (px << 8) | py;

                vPtr++;
                binAngleF += binAngleStep;
            }

            {
                vPtr->px = 0;
                vPtr->py = -qH;
                vPtr->pz = 0;
                vPtr->normal.data = detail::packNormalConstexpr(0.0f, -1.0f, 0.0f);
                vPtr++;
            }

            vertexCount = static_cast<uint16_t>(vPtr - vertices);

            Face *__restrict fPtr = faces;
            const uint16_t apexIdx = 0;
            const uint16_t baseCenterIdx = segs + 1;

            for (uint8_t i = 0; i < segs - 1; i++)
            {
                const uint16_t top1 = 1 + i;
                const uint16_t top2 = 2 + i;

                fPtr[0] = Face(apexIdx, top2, top1);
                fPtr[1] = Face(baseCenterIdx, top1, top2);
                fPtr += 2;
            }

            {
                const uint16_t i = segs - 1;
                const uint16_t top1 = 1 + i;
                const uint16_t top2 = 1;

                fPtr[0] = Face(apexIdx, top2, top1);
                fPtr[1] = Face(baseCenterIdx, top1, top2);
            }
            faceCount = segs * 2;

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
            cache.boundingCenter = Vector3(0.0f, yc, 0.0f);
            cache.boundingRadius = r_bound;
            cache.boundsValid = true;

            cache.transform.identity();
            cache.maxScale = 1.0f;
            cache.transformValid = true;
            transformDirty = false;
            cache.transformHash = 4216742517u;
        }
    };

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
            : Mesh(2 + (segments ? segments : 3) *
                           (((rings ? rings : 1) > 1 ? ((rings ? rings : 1) - 1) : 0) * 2 +
                            (((height - 2.0f * radius) > 0.0001f) ? 2 : 1)),
                   (segments ? segments : 3) *
                       ((((rings ? rings : 1) > 1 ? ((rings ? rings : 1) - 1) : 0) * 2 +
                         (((height - 2.0f * radius) > 0.0001f) ? 2 : 1) - 1) *
                            2 +
                        2),
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

            for (uint8_t ring = 1; ring < hemiRings; ++ring)
            {
                const float cosPhi = 1.0f - static_cast<float>(ring) * invHemiRings;
                const float sinPhi = sqrtf(1.0f - cosPhi * cosPhi);

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
                const float cosPhi = 1.0f - static_cast<float>(ring) * invHemiRings;
                const float sinPhi = sqrtf(1.0f - cosPhi * cosPhi);

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

            vertexCount = static_cast<uint16_t>(vPtr - vertices);

            Face *__restrict fPtr = faces;
            const uint16_t firstRingStart = 1;

            for (uint8_t seg = 0; seg < segs - 1; ++seg)
            {
                *fPtr++ = Face(topPoleIdx, firstRingStart + seg, firstRingStart + seg + 1);
            }
            *fPtr++ = Face(topPoleIdx, firstRingStart + segs - 1, firstRingStart);

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

                    fPtr[0] = Face(curr, below, currNext);
                    fPtr[1] = Face(currNext, below, belowNext);
                    fPtr += 2;
                }

                {
                    const uint16_t curr = currRow + segs - 1;
                    const uint16_t currNext = currRow;
                    const uint16_t below = nextRow + segs - 1;
                    const uint16_t belowNext = nextRow;

                    fPtr[0] = Face(curr, below, currNext);
                    fPtr[1] = Face(currNext, below, belowNext);
                    fPtr += 2;
                }
            }

            const uint16_t lastRingStart = 1 + (ringRows - 1) * segs;
            for (uint8_t seg = 0; seg < segs - 1; ++seg)
            {
                *fPtr++ = Face(bottomPoleIdx, lastRingStart + seg + 1, lastRingStart + seg);
            }
            *fPtr++ = Face(bottomPoleIdx, lastRingStart, lastRingStart + segs - 1);

            faceCount = static_cast<uint16_t>(fPtr - faces);

            cache.boundingCenter = Vector3(0.0f, 0.0f, 0.0f);
            cache.boundingRadius = height * 0.5f;
            cache.boundsValid = true;

            cache.transform.identity();
            cache.maxScale = 1.0f;
            cache.transformValid = true;
            transformDirty = false;
            cache.transformHash = 4216742517u;
        }
    };

    class TrefoilKnot : public Mesh
    {
    public:
        TrefoilKnot(float scale = 1.0f, uint8_t segments = 64, uint8_t tubeSegments = 12, const Color &color = Color::WHITE)
            : Mesh((segments ? segments : 3) * (tubeSegments ? tubeSegments : 3),
                   (segments ? segments : 3) * (tubeSegments ? tubeSegments : 3) * 2,
                   color)
        {
            autoScale(scale * 6.0f);

            if (!vertices || !faces)
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES,
                     "TrefoilKnot: Mesh base allocation failed (vertices=%p, faces=%p)",
                     static_cast<void *>(vertices),
                     static_cast<void *>(faces));
                return;
            }

            constexpr float tubeScale = 0.3f;
            const float tubeRadius = tubeScale * scale;
            uint8_t segs = segments ? segments : 3;
            uint8_t tubeSegs = tubeSegments ? tubeSegments : 3;

            for (uint8_t i = 0; i < segs; i++)
            {
                const float t = TWO_PI * i / segs;
                constexpr float two = 2.0f;
                constexpr float three = 3.0f;
                const float sin_t = FastMath::fastSin(t);
                const float cos_t = FastMath::fastCos(t);
                const float sin_2t = FastMath::fastSin(two * t);
                const float cos_2t = FastMath::fastCos(two * t);
                const float sin_3t = FastMath::fastSin(three * t);
                const float x = scale * (sin_t + two * sin_2t);
                const float y = scale * (cos_t - two * cos_2t);
                const float z = scale * (-sin_3t);
                for (uint8_t j = 0; j < tubeSegs; j++)
                {
                    const float angle = TWO_PI * j / tubeSegs;
                    const float nx = FastMath::fastCos(angle), ny = FastMath::fastSin(angle);
                    const float vx = x + tubeRadius * (nx * cos_t - ny * sin_t);
                    const float vy = y + tubeRadius * (nx * sin_t + ny * cos_t);
                    const float vz = z + tubeRadius * ny;

                    addVertex(Vector3(vx, vy, vz));
                }
            }

            for (uint8_t i = 0; i < segs; i++)
            {
                const uint16_t i1 = (i + 1) % segs;
                for (uint8_t j = 0; j < tubeSegs; j++)
                {
                    const uint16_t j1 = (j + 1) % tubeSegs;
                    const uint16_t a = i * tubeSegs + j, b = i1 * tubeSegs + j;
                    const uint16_t c = i1 * tubeSegs + j1, d = i * tubeSegs + j1;
                    addFace(a, b, c);
                    addFace(a, c, d);
                }
            }
            finalize();
        }

    private:
        inline void finalize()
        {
            finalizeNormals();
            calculateBoundingSphere();

            cache.transform.identity();
            cache.maxScale = 1.0f;
            cache.transformValid = true;
            transformDirty = false;
            cache.transformHash = 4216742517u;
        }
    };
}
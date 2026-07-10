#pragma once

#include "Geometry/Mesh.hpp"
#include "Math/Quant.hpp"

namespace pip3D
{
    namespace detail
    {
        alignas(16) static constexpr Vertex s_cubeVertices[24] = {
            {-32767, -32767, -32767, packNormalConstexpr(0.0f, 0.0f, -1.0f), 0.0f, 1.0f},
            { 32767, -32767, -32767, packNormalConstexpr(0.0f, 0.0f, -1.0f), 1.0f, 1.0f},
            { 32767,  32767, -32767, packNormalConstexpr(0.0f, 0.0f, -1.0f), 1.0f, 0.0f},
            {-32767,  32767, -32767, packNormalConstexpr(0.0f, 0.0f, -1.0f), 0.0f, 0.0f},
            { 32767, -32767,  32767, packNormalConstexpr(0.0f, 0.0f,  1.0f), 0.0f, 1.0f},
            {-32767, -32767,  32767, packNormalConstexpr(0.0f, 0.0f,  1.0f), 1.0f, 1.0f},
            {-32767,  32767,  32767, packNormalConstexpr(0.0f, 0.0f,  1.0f), 1.0f, 0.0f},
            { 32767,  32767,  32767, packNormalConstexpr(0.0f, 0.0f,  1.0f), 0.0f, 0.0f},
            {-32767,  32767, -32767, packNormalConstexpr(0.0f, 1.0f, 0.0f), 0.0f, 0.0f},
            { 32767,  32767, -32767, packNormalConstexpr(0.0f, 1.0f, 0.0f), 1.0f, 0.0f},
            { 32767,  32767,  32767, packNormalConstexpr(0.0f, 1.0f, 0.0f), 1.0f, 1.0f},
            {-32767,  32767,  32767, packNormalConstexpr(0.0f, 1.0f, 0.0f), 0.0f, 1.0f},
            {-32767, -32767,  32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 0.0f, 0.0f},
            { 32767, -32767,  32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 1.0f, 0.0f},
            { 32767, -32767, -32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 1.0f, 1.0f},
            {-32767, -32767, -32767, packNormalConstexpr(0.0f, -1.0f, 0.0f), 0.0f, 1.0f},
            { 32767, -32767, -32767, packNormalConstexpr(1.0f, 0.0f, 0.0f), 0.0f, 1.0f},
            { 32767, -32767,  32767, packNormalConstexpr(1.0f, 0.0f, 0.0f), 1.0f, 1.0f},
            { 32767,  32767,  32767, packNormalConstexpr(1.0f, 0.0f, 0.0f), 1.0f, 0.0f},
            { 32767,  32767, -32767, packNormalConstexpr(1.0f, 0.0f, 0.0f), 0.0f, 0.0f},
            {-32767, -32767,  32767, packNormalConstexpr(-1.0f, 0.0f, 0.0f), 0.0f, 1.0f},
            {-32767, -32767, -32767, packNormalConstexpr(-1.0f, 0.0f, 0.0f), 1.0f, 1.0f},
            {-32767,  32767, -32767, packNormalConstexpr(-1.0f, 0.0f, 0.0f), 1.0f, 0.0f},
            {-32767,  32767,  32767, packNormalConstexpr(-1.0f, 0.0f, 0.0f), 0.0f, 0.0f}
        };

        static constexpr Face s_cubeFaces[12] = {
            {0, 2, 1}, {0, 3, 2},
            {4, 6, 5}, {4, 7, 6},
            {8, 10, 9}, {8, 11, 10},
            {12, 14, 13}, {12, 15, 14},
            {16, 18, 17}, {16, 19, 18},
            {20, 22, 21}, {20, 23, 22}
        };

        alignas(16) static constexpr Vertex s_pyramidVertices[16] = {
            {    0,  32767,     0, packNormalConstexpr( 0.0f, 1.0f, -2.0f), 0.5f, 1.0f},
            {-32767, -32767, -32767, packNormalConstexpr( 0.0f, 1.0f, -2.0f), 0.0f, 0.0f},
            { 32767, -32767, -32767, packNormalConstexpr( 0.0f, 1.0f, -2.0f), 1.0f, 0.0f},
            {    0,  32767,     0, packNormalConstexpr( 2.0f, 1.0f,  0.0f), 0.5f, 1.0f},
            { 32767, -32767, -32767, packNormalConstexpr( 2.0f, 1.0f,  0.0f), 0.0f, 0.0f},
            { 32767, -32767,  32767, packNormalConstexpr( 2.0f, 1.0f,  0.0f), 1.0f, 0.0f},
            {    0,  32767,     0, packNormalConstexpr( 0.0f, 1.0f,  2.0f), 0.5f, 1.0f},
            { 32767, -32767,  32767, packNormalConstexpr( 0.0f, 1.0f,  2.0f), 0.0f, 0.0f},
            {-32767, -32767,  32767, packNormalConstexpr( 0.0f, 1.0f,  2.0f), 1.0f, 0.0f},
            {    0,  32767,     0, packNormalConstexpr(-2.0f, 1.0f,  0.0f), 0.5f, 1.0f},
            {-32767, -32767,  32767, packNormalConstexpr(-2.0f, 1.0f,  0.0f), 0.0f, 0.0f},
            {-32767, -32767, -32767, packNormalConstexpr(-2.0f, 1.0f,  0.0f), 1.0f, 0.0f},
            {-32767, -32767, -32767, packNormalConstexpr( 0.0f, -1.0f, 0.0f), 0.0f, 0.0f},
            { 32767, -32767, -32767, packNormalConstexpr( 0.0f, -1.0f, 0.0f), 1.0f, 0.0f},
            { 32767, -32767,  32767, packNormalConstexpr( 0.0f, -1.0f, 0.0f), 1.0f, 1.0f},
            {-32767, -32767,  32767, packNormalConstexpr( 0.0f, -1.0f, 0.0f), 0.0f, 1.0f}
        };

        static constexpr Face s_pyramidFaces[6] = {
            {0, 2, 1}, {3, 5, 4},
            {6, 8, 7}, {9, 11, 10},
            {12, 13, 14}, {12, 14, 15}
        };
    }

    class Cube : public Mesh
    {
    public:
        explicit Cube(float size = 1.0f)
            : Mesh(detail::s_cubeVertices, 24, detail::s_cubeFaces, 12, true)
        {
            autoScale(size);
            finalizeGeometry(24, 12, Vector3(0.0f, 0.0f, 0.0f), size * 0.8660254f);
            bindDeleter<Cube>();
        }
    };

    class Pyramid : public Mesh
    {
    public:
        explicit Pyramid(float size = 1.0f)
            : Mesh(detail::s_pyramidVertices, 16, detail::s_pyramidFaces, 6, true)
        {
            autoScale(size);
            finalizeGeometry(16, 6, Vector3(0.0f, -size * 0.25f, 0.0f), size * 0.75f);
            bindDeleter<Pyramid>();
        }
    };

    class Plane : public Mesh
    {
    public:
        Plane(float width = 2.0f, float depth = 2.0f, uint8_t subdivisions = 1,
              float uvScale = 1.0f)
            : Mesh(static_cast<uint16_t>(((subdivisions ? subdivisions : 1) + 1) *
                                         ((subdivisions ? subdivisions : 1) + 1)),
                   static_cast<uint16_t>((subdivisions ? subdivisions : 1) *
                                         (subdivisions ? subdivisions : 1) * 2))
        {
            setSingleColorLighting(true);

            const float size = (width > depth) ? width : depth;
            autoScale(size);
            if (unlikely(!vertices_ || !faces_))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Plane: alloc failed");
                return;
            }

            const uint8_t divs = subdivisions ? subdivisions : 1;
            const float ratioX = width / size;
            const float ratioZ = depth / size;
            const float qStartX = -ratioX * 32767.0f;
            const float qEndX   =  ratioX * 32767.0f;
            const float qStartZ = -ratioZ * 32767.0f;
            const float qEndZ   =  ratioZ * 32767.0f;
            const float stepX = (qEndX - qStartX) / divs;
            const float stepZ = (qEndZ - qStartZ) / divs;
            const float invDivs = 1.0f / static_cast<float>(divs);
            const float scaleUV = invDivs * uvScale;

            PackedNormal normalUp;
            normalUp.set(0.0f, 1.0f, 0.0f);
            const uint16_t normalUpData = normalUp.data;

            Vertex *PIP3D_RESTRICT vPtr = vertices_;
            for (uint8_t z = 0; z <= divs; ++z)
            {
                const float currentZ = qStartZ + static_cast<float>(z) * stepZ;
                const int16_t qZ = (z == divs)
                                       ? static_cast<int16_t>(lrintf(qEndZ))
                                       : static_cast<int16_t>(lrintf(currentZ));
                const float tv = static_cast<float>(z) * scaleUV;

                for (uint8_t x = 0; x <= divs; ++x)
                {
                    const float currentX = qStartX + static_cast<float>(x) * stepX;
                    const int16_t qX = (x == divs)
                                           ? static_cast<int16_t>(lrintf(qEndX))
                                           : static_cast<int16_t>(lrintf(currentX));

                    *vPtr = Vertex(qX, 0, qZ, normalUpData,
                                   static_cast<float>(x) * scaleUV, tv);
                    ++vPtr;
                }
            }

            Face *PIP3D_RESTRICT fPtr = faces_;
            const uint16_t pitch = divs + 1;
            uint16_t i0 = 0;
            uint16_t i1 = pitch;
            for (uint8_t z = 0; z < divs; ++z)
            {
                for (uint8_t x = 0; x < divs; ++x)
                {
                    fPtr[0] = Face(i0, i1, i0 + 1);
                    fPtr[1] = Face(i0 + 1, i1, i1 + 1);
                    fPtr += 2;
                    ++i0;
                    ++i1;
                }
                ++i0;
                ++i1;
            }

            const float diagSq = width * width + depth * depth;
            const float diag   = diagSq * FastMath::fastInvSqrt(diagSq);
            finalizeGeometry(static_cast<uint16_t>(vPtr - vertices_),
                             static_cast<uint16_t>(fPtr - faces_),
                             Vector3(0.0f, 0.0f, 0.0f),
                             0.5f * diag);
            bindDeleter<Plane>();
        }
    };

    class Cylinder : public Mesh
    {
    public:
        Cylinder(float radius = 1.0f, float height = 2.0f, uint8_t segments = 16,
                 float uvScaleU = 1.0f, float uvScaleV = 1.0f)
            : Mesh(static_cast<uint16_t>(4 + (segments ? segments : 3) * 4),
                   static_cast<uint16_t>((segments ? segments : 3) * 4))
        {
            const float size = (height > radius * 2.0f) ? height : radius * 2.0f;
            autoScale(size);
            if (unlikely(!vertices_ || !faces_))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Cylinder: alloc failed");
                return;
            }

            const uint8_t segs = segments ? segments : 3;
            const float h = height * 0.5f;
            const float halfSize = size * 0.5f;
            const float invHalfSize = FastMath::fastReciprocal(halfSize);
            const float scaleR = (radius * invHalfSize) * 32767.0f;
            const float scaleH = (h * invHalfSize) * 32767.0f;
            const int16_t qH  = static_cast<int16_t>(lrintf(scaleH));
            const int16_t mqH = static_cast<int16_t>(-qH);

            const uint16_t angleBinStep = 65536 / segs;
            const float invSegs = 1.0f / static_cast<float>(segs);
            const float scaleU  = invSegs * uvScaleU;

            PackedNormal topN; topN.set(0.0f,  1.0f, 0.0f);
            PackedNormal botN; botN.set(0.0f, -1.0f, 0.0f);
            const uint16_t topNData = topN.data;
            const uint16_t botNData = botN.data;

            Vertex *PIP3D_RESTRICT vPtr = vertices_;

            const uint16_t sideTopStart    = 0;
            const uint16_t sideBottomStart = segs + 1;
            const uint16_t topCenterIdx    = 2 * segs + 2;
            const uint16_t topCapStart     = 2 * segs + 3;
            const uint16_t bottomCenterIdx = 3 * segs + 3;
            const uint16_t bottomCapStart  = 3 * segs + 4;

            vPtr[topCenterIdx]    = Vertex(0, qH,  0, topNData, 0.5f, 0.5f);
            vPtr[bottomCenterIdx] = Vertex(0, mqH, 0, botNData, 0.5f, 0.5f);

            for (uint8_t i = 0; i <= segs; ++i)
            {
                const uint16_t angleBin = (i == segs) ? 0 : i * angleBinStep;
                float s, c;
                FastMath::fastSinCosBin(angleBin, s, c);
                const int16_t qRx = static_cast<int16_t>(lrintf(c * scaleR));
                const int16_t qRz = static_cast<int16_t>(lrintf(s * scaleR));
                const float u = static_cast<float>(i) * scaleU;

                PackedNormal sideN;
                sideN.set(c, 0.0f, s);
                const uint16_t sideNData = sideN.data;

                vPtr[sideTopStart + i]    = Vertex(qRx, qH,  qRz, sideNData, u, 0.0f);
                vPtr[sideBottomStart + i] = Vertex(qRx, mqH, qRz, sideNData, u, uvScaleV);

                if (i < segs)
                {
                    const float cu = 0.5f + 0.5f * c;
                    const float cv = 0.5f + 0.5f * s;
                    vPtr[topCapStart + i]    = Vertex(qRx, qH,  qRz, topNData, cu, cv);
                    vPtr[bottomCapStart + i] = Vertex(qRx, mqH, qRz, botNData, cu, cv);
                }
            }

            Face *PIP3D_RESTRICT fPtr = faces_;
            uint16_t fIdx = 0;
            for (uint8_t i = 0; i < segs; ++i)
            {
                const uint16_t next = i + 1;
                const uint16_t t1 = sideTopStart + i;
                const uint16_t t2 = sideTopStart + next;
                const uint16_t b1 = sideBottomStart + i;
                const uint16_t b2 = sideBottomStart + next;

                fPtr[fIdx++] = Face(t1, t2, b1);
                fPtr[fIdx++] = Face(t2, b2, b1);

                const uint16_t capNext = (i + 1 == segs) ? 0 : i + 1;
                fPtr[fIdx++] = Face(topCenterIdx, topCapStart + capNext, topCapStart + i);
                fPtr[fIdx++] = Face(bottomCenterIdx, bottomCapStart + i, bottomCapStart + capNext);
            }

            const float diagSq = radius * radius + h * h;
            const float diag   = diagSq * FastMath::fastInvSqrt(diagSq);
            finalizeGeometry(static_cast<uint16_t>(4 + segs * 4),
                             static_cast<uint16_t>(segs * 4),
                             Vector3(0.0f, 0.0f, 0.0f), diag);
            bindDeleter<Cylinder>();
        }
    };

    class Cone : public Mesh
    {
    public:
        Cone(float radius = 1.0f, float height = 2.0f, uint8_t segments = 16,
             float uvScaleU = 1.0f, float uvScaleV = 1.0f)
            : Mesh(static_cast<uint16_t>(3 + (segments ? segments : 3) * 3),
                   static_cast<uint16_t>((segments ? segments : 3) * 2))
        {
            const float size = (height > radius * 2.0f) ? height : radius * 2.0f;
            autoScale(size);
            if (unlikely(!vertices_ || !faces_))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Cone: alloc failed");
                return;
            }

            const uint8_t segs = segments ? segments : 3;
            const float h = height * 0.5f;
            const float halfSize = size * 0.5f;
            const float invHalfSize = FastMath::fastReciprocal(halfSize);
            const float scaleR = (radius * invHalfSize) * 32767.0f;
            const float scaleH = (h * invHalfSize) * 32767.0f;
            const int16_t qH  = static_cast<int16_t>(lrintf(scaleH));
            const int16_t mqH = static_cast<int16_t>(-qH);

            const uint16_t angleBinStep = 65536 / segs;
            const float invSegs = 1.0f / static_cast<float>(segs);
            const float scaleU  = invSegs * uvScaleU;

            PackedNormal botN; botN.set(0.0f, -1.0f, 0.0f);
            const uint16_t botNData = botN.data;

            Vertex *PIP3D_RESTRICT vPtr = vertices_;

            const uint16_t sideApexStart  = 0;
            const uint16_t sideBaseStart  = segs + 1;
            const uint16_t baseCenterIdx  = 2 * segs + 2;
            const uint16_t bottomCapStart = 2 * segs + 3;

            vPtr[baseCenterIdx] = Vertex(0, mqH, 0, botNData, 0.5f, 0.5f);

            for (uint8_t i = 0; i <= segs; ++i)
            {
                const uint16_t angleBin = (i == segs) ? 0 : i * angleBinStep;
                float s, c;
                FastMath::fastSinCosBin(angleBin, s, c);
                const int16_t qRx = static_cast<int16_t>(lrintf(c * scaleR));
                const int16_t qRz = static_cast<int16_t>(lrintf(s * scaleR));
                const float u = static_cast<float>(i) * scaleU;

                PackedNormal sideN;
                sideN.set(height * c, -radius, height * s);
                const uint16_t sideNData = sideN.data;

                vPtr[sideApexStart + i] = Vertex(0, qH, 0, sideNData, u, 0.0f);
                vPtr[sideBaseStart + i] = Vertex(qRx, mqH, qRz, sideNData, u, uvScaleV);

                if (i < segs)
                {
                    vPtr[bottomCapStart + i] = Vertex(qRx, mqH, qRz, botNData,
                                                      0.5f + 0.5f * c, 0.5f + 0.5f * s);
                }
            }

            Face *PIP3D_RESTRICT fPtr = faces_;
            uint16_t fIdx = 0;
            for (uint8_t i = 0; i < segs; ++i)
            {
                const uint16_t next = i + 1;
                const uint16_t apex = sideApexStart + i;
                const uint16_t b1 = sideBaseStart + i;
                const uint16_t b2 = sideBaseStart + next;

                fPtr[fIdx++] = Face(apex, b2, b1);

                const uint16_t capNext = (i + 1 == segs) ? 0 : i + 1;
                fPtr[fIdx++] = Face(baseCenterIdx, bottomCapStart + i, bottomCapStart + capNext);
            }

            float yc, r_bound;
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

            finalizeGeometry(static_cast<uint16_t>(3 + segs * 3),
                             static_cast<uint16_t>(segs * 2),
                             Vector3(0.0f, yc, 0.0f), r_bound);
            bindDeleter<Cone>();
        }
    };
}
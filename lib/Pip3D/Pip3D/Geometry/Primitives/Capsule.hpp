#pragma once

#include "Geometry/Mesh.hpp"
#include "Math/Quant.hpp"

namespace pip3D
{
    namespace detail
    {
        static constexpr uint16_t capsuleVertexCount(uint8_t segments, uint8_t rings,
                                                     float height, float radius)
        {
            const uint8_t segs = segments ? segments : 3;
            const uint8_t hemiRings = rings ? rings : 1;
            const bool hasCyl = (height - 2.0f * radius) > 0.0001f;
            const uint16_t ringRows = 2 * (hemiRings - 1) + 1 + (hasCyl ? 1 : 0);
            return 2 * segs + ringRows * (segs + 1);
        }

        static constexpr uint16_t capsuleFaceCount(uint8_t segments, uint8_t rings,
                                                   float height, float radius)
        {
            const uint8_t segs = segments ? segments : 3;
            const uint8_t hemiRings = rings ? rings : 1;
            const bool hasCyl = (height - 2.0f * radius) > 0.0001f;
            const uint16_t ringRows = 2 * (hemiRings - 1) + 1 + (hasCyl ? 1 : 0);
            return 2 * segs * ringRows;
        }
    }

    class Capsule : public Mesh
    {
    public:
        Capsule(float radius = 1.0f, float height = 2.0f,
                uint8_t segments = 12, uint8_t rings = 6)
            : Mesh(detail::capsuleVertexCount(segments, rings, height, radius),
                   detail::capsuleFaceCount(segments, rings, height, radius))
        {
            const float size = (height > radius * 2.0f) ? height : radius * 2.0f;
            autoScale(size);
            if (unlikely(!vertices_ || !faces_))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Capsule: alloc failed");
                return;
            }

            const float cylinderHeight = fmaxf(0.0f, height - 2.0f * radius);
            const float halfCyl = cylinderHeight * 0.5f;
            const uint8_t segs = (segments == 0) ? 3 : (segments > 64 ? 64 : segments);
            const uint8_t hemiRings = (rings == 0) ? 1 : (rings > 16 ? 16 : rings);
            const bool hasCylinder = cylinderHeight > 0.0001f;

            const float halfSize = size * 0.5f;
            const float invHalfSize = FastMath::fastReciprocal(halfSize);
            const float scaleR = (radius * invHalfSize) * 32767.0f;
            const float scaleCyl = (halfCyl * invHalfSize) * 32767.0f;

            const uint16_t ringRows = 2 * (hemiRings - 1) + 1 + (hasCylinder ? 1 : 0);
            const uint16_t ringStride = segs + 1;

            const uint16_t topPoleStart = 0;
            const uint16_t ringStart = segs;
            const uint16_t bottomPoleStart = ringStart + ringRows * ringStride;

            const float invSegs = FastMath::fastReciprocal(static_cast<float>(segs));
            const float total_L = kPi * scaleR + (hasCylinder ? 2.0f * scaleCyl : 0.0f);
            const float invTotal_L = FastMath::fastReciprocal(total_L);
            const float phi_step = (kPi * 0.5f) * FastMath::fastReciprocal(static_cast<float>(hemiRings));
            const float halfPiScaleR_invTotal = (kPi * 0.5f) * scaleR * invTotal_L;

            float sinThetaCache[64];
            float cosThetaCache[64];
            const uint16_t angleBinStep = 65536 / segs;
            for (uint8_t j = 0; j < segs; ++j)
                FastMath::fastSinCosBin(j * angleBinStep, sinThetaCache[j], cosThetaCache[j]);

            float sinPhiCache[16];
            float cosPhiCache[16];
            for (uint8_t ring = 1; ring < hemiRings; ++ring)
            {
                const float phi = static_cast<float>(ring) * phi_step;
                FastMath::fastSinCos(phi, sinPhiCache[ring - 1], cosPhiCache[ring - 1]);
            }

            constexpr uint16_t poleTopNData = packNormalConstexpr(0.0f, 1.0f, 0.0f);
            constexpr uint16_t poleBotNData = packNormalConstexpr(0.0f, -1.0f, 0.0f);

            Vertex *PIP3D_RESTRICT vPtr = vertices_;

            const int16_t topPoleY = static_cast<int16_t>(lrintf(scaleCyl + scaleR));
            for (uint8_t seg = 0; seg < segs; ++seg)
            {
                *vPtr++ = Vertex(0, topPoleY, 0, poleTopNData,
                                 (static_cast<float>(seg) + 0.5f) * invSegs, 0.0f);
            }

            for (uint8_t ring = 1; ring < hemiRings; ++ring)
            {
                const float sinPhi = sinPhiCache[ring - 1];
                const float cosPhi = cosPhiCache[ring - 1];
                const float phi = static_cast<float>(ring) * phi_step;
                const int16_t qY = static_cast<int16_t>(lrintf(scaleCyl + scaleR * cosPhi));
                const float r_scale = scaleR * sinPhi;
                const float v = phi * scaleR * invTotal_L;

                for (uint8_t seg = 0; seg < segs; ++seg)
                {
                    const float cosT = cosThetaCache[seg];
                    const float sinT = sinThetaCache[seg];

                    PackedNormal n;
                    n.set(sinPhi * cosT, cosPhi, sinPhi * sinT);

                    *vPtr++ = Vertex(static_cast<int16_t>(lrintf(r_scale * cosT)),
                                     qY,
                                     static_cast<int16_t>(lrintf(r_scale * sinT)),
                                     n.data,
                                     static_cast<float>(seg) * invSegs, v);
                }

                PackedNormal n0;
                n0.set(sinPhi, cosPhi, 0.0f);
                *vPtr++ = Vertex(static_cast<int16_t>(lrintf(r_scale)),
                                 qY,
                                 0,
                                 n0.data, 1.0f, v);
            }

            {
                const int16_t qY = static_cast<int16_t>(lrintf(scaleCyl));
                const float v = halfPiScaleR_invTotal;
                for (uint8_t seg = 0; seg < segs; ++seg)
                {
                    const float cosT = cosThetaCache[seg];
                    const float sinT = sinThetaCache[seg];

                    PackedNormal n;
                    n.set(cosT, 0.0f, sinT);

                    *vPtr++ = Vertex(static_cast<int16_t>(lrintf(scaleR * cosT)),
                                     qY,
                                     static_cast<int16_t>(lrintf(scaleR * sinT)),
                                     n.data,
                                     static_cast<float>(seg) * invSegs, v);
                }
                PackedNormal n0;
                n0.set(1.0f, 0.0f, 0.0f);
                *vPtr++ = Vertex(static_cast<int16_t>(lrintf(scaleR)),
                                 qY,
                                 0,
                                 n0.data, 1.0f, v);
            }

            if (hasCylinder)
            {
                const int16_t qY = static_cast<int16_t>(lrintf(-scaleCyl));
                const float v = 1.0f - halfPiScaleR_invTotal;
                for (uint8_t seg = 0; seg < segs; ++seg)
                {
                    const float cosT = cosThetaCache[seg];
                    const float sinT = sinThetaCache[seg];

                    PackedNormal n;
                    n.set(cosT, 0.0f, sinT);

                    *vPtr++ = Vertex(static_cast<int16_t>(lrintf(scaleR * cosT)),
                                     qY,
                                     static_cast<int16_t>(lrintf(scaleR * sinT)),
                                     n.data,
                                     static_cast<float>(seg) * invSegs, v);
                }
                PackedNormal n0;
                n0.set(1.0f, 0.0f, 0.0f);
                *vPtr++ = Vertex(static_cast<int16_t>(lrintf(scaleR)),
                                 qY,
                                 0,
                                 n0.data, 1.0f, v);
            }

            for (int ring = static_cast<int>(hemiRings) - 1; ring >= 1; --ring)
            {
                const float sinPhi = sinPhiCache[ring - 1];
                const float cosPhi = cosPhiCache[ring - 1];
                const float phi = static_cast<float>(ring) * phi_step;
                const int16_t qY = static_cast<int16_t>(lrintf(-scaleCyl - scaleR * cosPhi));
                const float r_scale = scaleR * sinPhi;
                const float v = 1.0f - phi * scaleR * invTotal_L;

                for (uint8_t seg = 0; seg < segs; ++seg)
                {
                    const float cosT = cosThetaCache[seg];
                    const float sinT = sinThetaCache[seg];

                    PackedNormal n;
                    n.set(sinPhi * cosT, -cosPhi, sinPhi * sinT);

                    *vPtr++ = Vertex(static_cast<int16_t>(lrintf(r_scale * cosT)),
                                     qY,
                                     static_cast<int16_t>(lrintf(r_scale * sinT)),
                                     n.data,
                                     static_cast<float>(seg) * invSegs, v);
                }
                PackedNormal n0;
                n0.set(sinPhi, -cosPhi, 0.0f);
                *vPtr++ = Vertex(static_cast<int16_t>(lrintf(r_scale)),
                                 qY,
                                 0,
                                 n0.data, 1.0f, v);
            }

            const int16_t botPoleY = static_cast<int16_t>(lrintf(-scaleCyl - scaleR));
            for (uint8_t seg = 0; seg < segs; ++seg)
            {
                *vPtr++ = Vertex(0, botPoleY, 0, poleBotNData,
                                 (static_cast<float>(seg) + 0.5f) * invSegs, 1.0f);
            }

            const uint16_t vCount = static_cast<uint16_t>(vPtr - vertices_);

            Face *PIP3D_RESTRICT fPtr = faces_;

            for (uint8_t seg = 0; seg < segs; ++seg)
            {
                const uint16_t poleIdx = topPoleStart + seg;
                const uint16_t r0 = ringStart + seg;
                const uint16_t r1 = ringStart + seg + 1;
                *fPtr++ = Face(poleIdx, r1, r0);
            }

            for (uint16_t ring = 0; ring < ringRows - 1; ++ring)
            {
                const uint16_t currRow = ringStart + ring * ringStride;
                const uint16_t nextRow = currRow + ringStride;

                for (uint8_t seg = 0; seg < segs; ++seg)
                {
                    const uint16_t curr = currRow + seg;
                    const uint16_t below = nextRow + seg;
                    fPtr[0] = Face(curr, curr + 1, below);
                    fPtr[1] = Face(curr + 1, below + 1, below);
                    fPtr += 2;
                }
            }

            const uint16_t lastRingStart = ringStart + (ringRows - 1) * ringStride;
            for (uint8_t seg = 0; seg < segs; ++seg)
            {
                const uint16_t poleIdx = bottomPoleStart + seg;
                const uint16_t r0 = lastRingStart + seg;
                const uint16_t r1 = lastRingStart + seg + 1;
                *fPtr++ = Face(poleIdx, r0, r1);
            }

            const uint16_t fCount = static_cast<uint16_t>(fPtr - faces_);

            float boundRadius;
            if (hasCylinder)
            {
                const float boundSq = radius * radius + halfCyl * halfCyl;
                boundRadius = boundSq * FastMath::fastInvSqrt(boundSq);
            }
            else
            {
                boundRadius = radius;
            }
            finalizeGeometry(vCount, fCount, Vector3(0.0f, 0.0f, 0.0f), boundRadius);
            bindDeleter<Capsule>();
        }
    };

}

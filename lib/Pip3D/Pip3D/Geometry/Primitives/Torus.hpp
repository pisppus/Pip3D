#pragma once

#include "Geometry/Mesh.hpp"
#include "Math/Quant.hpp"

namespace pip3D
{
    class Torus : public Mesh
    {
    private:
        static PIP3D_FORCE_INLINE int16_t saturateInt16(int32_t v)
        {
            if (v < -32768)
                return -32768;
            if (v > 32767)
                return 32767;
            return static_cast<int16_t>(v);
        }

    public:
        Torus(float majorRadius = 1.0f, float minorRadius = 0.3f,
              uint8_t segments = 16, uint8_t tubeSegments = 8,
              float uvScaleU = 1.0f, float uvScaleV = 1.0f)
            : Mesh(static_cast<uint16_t>((segments ? segments : 3)) *
                       static_cast<uint16_t>((tubeSegments ? tubeSegments : 3)),
                   static_cast<uint16_t>((segments ? segments : 3) *
                                         (tubeSegments ? tubeSegments : 3) * 2))
        {
            const uint8_t segs = (segments == 0) ? 3 : (segments > 64 ? 64 : segments);
            const uint8_t tubeSegs = (tubeSegments == 0) ? 3 : (tubeSegments > 64 ? 64 : tubeSegments);

            const float size = (majorRadius + minorRadius) * 2.0f;
            autoScale(size);
            if (unlikely(!vertices_ || !faces_))
            {
                LOGE(::pip3D::Debug::LOG_MODULE_RESOURCES, "Torus: alloc failed");
                return;
            }

            const float invHalfSize = FastMath::fastReciprocal(majorRadius + minorRadius);
            const float scaleMajor = (majorRadius * invHalfSize) * 32767.0f;
            const float scaleMinor = (minorRadius * invHalfSize) * 32767.0f;

            const uint16_t majorBinStep = 65536 / segs;
            const uint16_t minorBinStep = 65536 / tubeSegs;

            float sinU[64], cosU[64];
            float sinV[64], cosV[64];
            for (uint8_t i = 0; i < segs; ++i)
                FastMath::fastSinCosBin(static_cast<uint16_t>(i) * majorBinStep,
                                        sinU[i], cosU[i]);
            for (uint8_t j = 0; j < tubeSegs; ++j)
                FastMath::fastSinCosBin(static_cast<uint16_t>(j) * minorBinStep,
                                        sinV[j], cosV[j]);

            const float invSegs = FastMath::fastReciprocal(static_cast<float>(segs));
            const float invTubeSegs = FastMath::fastReciprocal(static_cast<float>(tubeSegs));

            Vertex *PIP3D_RESTRICT vPtr = vertices_;

            for (uint8_t i = 0; i < segs; ++i)
            {
                const float cu = cosU[i];
                const float su = sinU[i];
                const float u = static_cast<float>(i) * invSegs * uvScaleU;

                const float ringCenterX = scaleMajor * cu;
                const float ringCenterZ = scaleMajor * su;

                for (uint8_t j = 0; j < tubeSegs; ++j)
                {
                    const float cv = cosV[j];
                    const float sv = sinV[j];
                    const float v = static_cast<float>(j) * invTubeSegs * uvScaleV;

                    const float nx = cv * cu;
                    const float ny = sv;
                    const float nz = cv * su;

                    PackedNormal n;
                    n.set(nx, ny, nz);

                    *vPtr = Vertex(
                        saturateInt16(lrintf(ringCenterX + scaleMinor * nx)),
                        saturateInt16(lrintf(scaleMinor * ny)),
                        saturateInt16(lrintf(ringCenterZ + scaleMinor * nz)),
                        n.data, u, v);
                    ++vPtr;
                }
            }

            Face *PIP3D_RESTRICT fPtr = faces_;
            const uint16_t pitch = tubeSegs;
            for (uint8_t i = 0; i < segs; ++i)
            {
                const uint16_t iNext = static_cast<uint16_t>((i + 1) % segs);
                const uint16_t rowCurr = static_cast<uint16_t>(i) * pitch;
                const uint16_t rowNext = iNext * pitch;

                for (uint8_t j = 0; j < tubeSegs; ++j)
                {
                    const uint16_t jNext = static_cast<uint16_t>((j + 1) % tubeSegs);

                    const uint16_t a = rowCurr + j;
                    const uint16_t b = rowNext + j;
                    const uint16_t c = rowNext + jNext;
                    const uint16_t d = rowCurr + jNext;

                    fPtr[0] = Face(a, c, b);
                    fPtr[1] = Face(a, d, c);
                    fPtr += 2;
                }
            }

            const float boundRadius = majorRadius + minorRadius;
            finalizeGeometry(static_cast<uint16_t>(vPtr - vertices_),
                             static_cast<uint16_t>(fPtr - faces_),
                             Vector3(0.0f, 0.0f, 0.0f),
                             boundRadius);
            bindDeleter<Torus>();
        }
    };

}
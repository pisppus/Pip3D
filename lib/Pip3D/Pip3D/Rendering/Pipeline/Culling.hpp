#pragma once

#include "Core/Platform.hpp"
#include "Camera/Camera.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"

namespace pip3D
{
    namespace Culling
    {
        inline bool IRAM_ATTR isInstanceOccluded(
            const Vector3 &center,
            float radius,
            const Camera &camera,
            const Viewport &viewport,
            const Matrix4x4 &viewProjMatrix,
            ZBuffer *zBuffer,
            const DisplayConfig &cfg,
            const Vector3 &camFwd,
            float zEye,
            float radiusPixels)
        {

            if (unlikely(zEye - radius <= camera.nearPlane))
                return false;

            const float *PIP3D_RESTRICT m = viewProjMatrix.m;

            const float cx_w = center.x;
            const float cy_w = center.y;
            const float cz_w = center.z;

            const float clipX = m[0] * cx_w + m[4] * cy_w + m[8] * cz_w + m[12];
            const float clipY = m[1] * cx_w + m[5] * cy_w + m[9] * cz_w + m[13];
            const float clipW = m[3] * cx_w + m[7] * cy_w + m[11] * cz_w + m[15];

            const float invW = FastMath::fastReciprocal(clipW);

            const float halfW = static_cast<float>(viewport.width) * 0.5f;
            const float halfH = static_cast<float>(viewport.height) * 0.5f;

            const float cx_f = clipX * (invW * halfW) + halfW;
            const float cy_f = -clipY * (invW * halfH) + halfH;

            const int16_t cx = static_cast<int16_t>(cx_f);
            const int16_t cy = static_cast<int16_t>(cy_f);
            const int16_t rScrInt = static_cast<int16_t>(fmaxf(radiusPixels, 1.0f));

            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandH = static_cast<int16_t>(cfg.height);
            const uint16_t cfgW = cfg.width;
            const uint16_t bandHU = static_cast<uint16_t>(bandH);

            const int16_t localCy = cy - bandTop;
            const uint16_t localCyU = static_cast<uint16_t>(localCy);

            const bool cyInBand = (localCyU < bandHU);
            const bool cxInScr = (static_cast<uint16_t>(cx) < cfgW);

            const uint16_t *PIP3D_RESTRICT zb = zBuffer->data();

            const size_t rowCenter = static_cast<size_t>(localCyU) * SCREEN_WIDTH;

            const uint16_t rScrU = static_cast<uint16_t>(rScrInt);
            const size_t rowDown = static_cast<size_t>(localCyU + rScrU) * SCREEN_WIDTH;
            const size_t rowUp = static_cast<size_t>(localCyU - rScrU) * SCREEN_WIDTH;

            PIP3D_PREFETCH_R(zb + rowCenter + static_cast<size_t>(cx));
            PIP3D_PREFETCH_R(zb + rowDown + static_cast<size_t>(cx));
            PIP3D_PREFETCH_R(zb + rowUp + static_cast<size_t>(cx));

            const float fx = cx_w - camFwd.x * radius;
            const float fy = cy_w - camFwd.y * radius;
            const float fz = cz_w - camFwd.z * radius;

            const float clipWf = m[3] * fx + m[7] * fy + m[11] * fz + m[15];
            const float invFrontW = FastMath::fastReciprocal(clipWf);
            const float objDepth = g_wBufferScale * invFrontW;
            const uint16_t objDepthInt = static_cast<uint16_t>(objDepth);

            bool anyOccluded = false;

            if (cyInBand && cxInScr)
            {
                const uint16_t d = zb[rowCenter + static_cast<size_t>(cx)] & Z_DEPTH_MASK;
                if (d <= objDepthInt)
                    return false;
                anyOccluded = true;
            }

            {
                const int16_t sxR = cx + rScrInt;
                const uint16_t sxRU = static_cast<uint16_t>(sxR);
                if (cyInBand && sxRU < cfgW)
                {
                    const uint16_t d = zb[rowCenter + static_cast<size_t>(sxRU)] & Z_DEPTH_MASK;
                    if (d <= objDepthInt)
                        return false;
                    anyOccluded = true;
                }
            }

            {
                const int16_t sxL = cx - rScrInt;
                const uint16_t sxLU = static_cast<uint16_t>(sxL);
                if (cyInBand && sxLU < cfgW)
                {
                    const uint16_t d = zb[rowCenter + static_cast<size_t>(sxLU)] & Z_DEPTH_MASK;
                    if (d <= objDepthInt)
                        return false;
                    anyOccluded = true;
                }
            }

            {
                const int16_t syD = localCy + rScrInt;
                const uint16_t syDU = static_cast<uint16_t>(syD);
                if (syDU < bandHU && cxInScr)
                {
                    const uint16_t d = zb[rowDown + static_cast<size_t>(cx)] & Z_DEPTH_MASK;
                    if (d <= objDepthInt)
                        return false;
                    anyOccluded = true;
                }
            }

            {
                const int16_t syU = localCy - rScrInt;
                const uint16_t syUU = static_cast<uint16_t>(syU);
                if (syUU < bandHU && cxInScr)
                {
                    const uint16_t d = zb[rowUp + static_cast<size_t>(cx)] & Z_DEPTH_MASK;
                    if (d <= objDepthInt)
                        return false;
                    anyOccluded = true;
                }
            }

            return anyOccluded;
        }
    }
}

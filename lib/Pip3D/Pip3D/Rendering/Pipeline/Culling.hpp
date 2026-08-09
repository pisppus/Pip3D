#pragma once

#include "Core/Platform.hpp"
#include "Camera/Camera.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"

namespace pip3D
{
    namespace Culling
    {

        namespace detail
        {
            inline float g_projScale = 1.0f;
            inline float g_lastFov = -1.0f;
            inline uint16_t g_lastVpH = 0;
        }

        PIP3D_FORCE_INLINE float ensureProjScale(const Camera &cam,
                                                 const Viewport &vp) noexcept
        {
            if (likely(cam.fov == detail::g_lastFov && vp.height == detail::g_lastVpH))
                return detail::g_projScale;

            detail::g_lastFov = cam.fov;
            detail::g_lastVpH = vp.height;
            float s, c;
            FastMath::fastSinCos(cam.fov * 0.5f * kDegToRad, s, c);
            detail::g_projScale = c * FastMath::fastReciprocal(s) *
                                  (static_cast<float>(vp.height) * 0.5f);
            return detail::g_projScale;
        }

        PIP3D_FORCE_INLINE float computeEyeZ(const Vector3 &center,
                                             const Vector3 &camPos,
                                             const Vector3 &camFwd) noexcept
        {
            const float dx = center.x - camPos.x;
            const float dy = center.y - camPos.y;
            const float dz = center.z - camPos.z;
            return dx * camFwd.x + dy * camFwd.y + dz * camFwd.z;
        }

        PIP3D_FORCE_INLINE float computeScreenRadius(float worldRadius,
                                                     float eyeZ,
                                                     float projScale) noexcept
        {
            return worldRadius * projScale * FastMath::fastReciprocal(eyeZ);
        }

        inline bool IRAM_ATTR isInstanceOccluded(
            const Vector3 &center,
            float radius,
            float eyeZ,
            float radiusPixels,
            const Viewport &viewport,
            const Matrix4x4 &viewProjMatrix,
            const ZBuffer *PIP3D_RESTRICT zBuffer) noexcept
        {
            if (unlikely(eyeZ <= radius))
                return false;

            const float *PIP3D_RESTRICT m = viewProjMatrix.m;

            const float clipX = m[0] * center.x + m[4] * center.y + m[8] * center.z + m[12];
            const float clipY = m[1] * center.x + m[5] * center.y + m[9] * center.z + m[13];

            const float invW = FastMath::fastReciprocal(eyeZ);

            const float halfW = static_cast<float>(viewport.width) * 0.5f;
            const float halfH = static_cast<float>(viewport.height) * 0.5f;

            const float cx_f = clipX * (invW * halfW) + halfW;
            const float cy_f = -clipY * (invW * halfH) + halfH;

            const int16_t cx = static_cast<int16_t>(cx_f);
            const int16_t cy = static_cast<int16_t>(cy_f);

            const int16_t rScrInt = (radiusPixels < 1.0f) ? 1
                                                          : static_cast<int16_t>(radiusPixels);

            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandH = g_bandHeight;
            const int16_t localCy = cy - bandTop;
            const int16_t screenW = static_cast<int16_t>(viewport.width);

            if (localCy + rScrInt < 0 || localCy - rScrInt >= bandH)
                return false;

            const float frontEyeZ = eyeZ - radius;
            const float objDepth = g_wBufferScale * FastMath::fastReciprocal(frontEyeZ);
            const uint16_t objDepthInt = static_cast<uint16_t>(objDepth);

            const uint16_t *PIP3D_RESTRICT zb = zBuffer->data();

            const int16_t localCyClamped = (localCy < 0) ? 0
                                                         : ((localCy >= bandH) ? (bandH - 1) : localCy);
            const int16_t rowDownY = localCy + rScrInt;
            const int16_t rowUpY = localCy - rScrInt;
            const int16_t rowDownClamped = (rowDownY >= bandH) ? (bandH - 1)
                                                               : ((rowDownY < 0) ? 0 : rowDownY);
            const int16_t rowUpClamped = (rowUpY < 0) ? 0
                                                      : ((rowUpY >= bandH) ? (bandH - 1) : rowUpY);

            const size_t rowCenter = static_cast<size_t>(localCyClamped) * SCREEN_WIDTH;
            const size_t rowDown = static_cast<size_t>(rowDownClamped) * SCREEN_WIDTH;
            const size_t rowUp = static_cast<size_t>(rowUpClamped) * SCREEN_WIDTH;

            PIP3D_PREFETCH_R(zb + rowCenter + static_cast<size_t>(cx));
            PIP3D_PREFETCH_R(zb + rowDown + static_cast<size_t>(cx));
            PIP3D_PREFETCH_R(zb + rowUp + static_cast<size_t>(cx));

            const uint16_t bandHU = static_cast<uint16_t>(bandH);
            const uint16_t screenWU = static_cast<uint16_t>(screenW);

            const bool cyInBand = (static_cast<uint16_t>(localCy) < bandHU);
            const bool cxInScr = (static_cast<uint16_t>(cx) < screenWU);
            const bool cyDownInBand = (static_cast<uint16_t>(rowDownY) < bandHU);
            const bool cyUpInBand = (static_cast<uint16_t>(rowUpY) < bandHU);

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
                if (cyInBand && static_cast<uint16_t>(sxR) < screenWU)
                {
                    const uint16_t d = zb[rowCenter + static_cast<size_t>(sxR)] & Z_DEPTH_MASK;
                    if (d <= objDepthInt)
                        return false;
                    anyOccluded = true;
                }
            }

            {
                const int16_t sxL = cx - rScrInt;
                if (cyInBand && static_cast<uint16_t>(sxL) < screenWU)
                {
                    const uint16_t d = zb[rowCenter + static_cast<size_t>(sxL)] & Z_DEPTH_MASK;
                    if (d <= objDepthInt)
                        return false;
                    anyOccluded = true;
                }
            }

            if (cyDownInBand && cxInScr)
            {
                const uint16_t d = zb[rowDown + static_cast<size_t>(cx)] & Z_DEPTH_MASK;
                if (d <= objDepthInt)
                    return false;
                anyOccluded = true;
            }

            if (cyUpInBand && cxInScr)
            {
                const uint16_t d = zb[rowUp + static_cast<size_t>(cx)] & Z_DEPTH_MASK;
                if (d <= objDepthInt)
                    return false;
                anyOccluded = true;
            }

            return anyOccluded;
        }
    }
}

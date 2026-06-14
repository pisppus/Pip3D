#pragma once

#include "Core/Platform.hpp"
#include "Core/Viewport.hpp"
#include "Camera/Camera.hpp"
#include "Math/Algebra.hpp"
#include "Rendering/Display/ZBuffer.hpp"

namespace pip3D
{
    struct CullingCache
    {
        float projFactor = 1.0f;
        float lastFov = -1.0f;
        float lastOrthoH = -1.0f;
        uint16_t lastVpW = 0;
        uint16_t lastVpH = 0;

        IRAM_ATTR void update(const Camera &cam, const Viewport &vp)
        {
            if (cam.projectionType == PERSPECTIVE)
            {
                if (unlikely(cam.fov != lastFov || vp.height != lastVpH))
                {
                    lastFov = cam.fov;
                    lastVpH = vp.height;
                    float s, c;
                    FastMath::fastSinCos(cam.fov * 0.5f * kDegToRad, s, c);
                    projFactor = c * FastMath::fastReciprocal(s) * (static_cast<float>(vp.height) * 0.5f);
                }
            }
            else
            {
                if (unlikely(cam.orthoHeight != lastOrthoH ||
                             vp.width != lastVpW ||
                             vp.height != lastVpH))
                {
                    lastOrthoH = cam.orthoHeight;
                    lastVpW = vp.width;
                    lastVpH = vp.height;

                    const float aspect = static_cast<float>(vp.width) * FastMath::fastReciprocal(static_cast<float>(vp.height));
                    const float aspectFactor = fmaxf(1.0f, aspect);

                    projFactor = static_cast<float>(vp.height) * aspectFactor * FastMath::fastReciprocal(cam.orthoHeight);
                }
            }
        }
    };

    class Culling
    {
    public:
        static bool IRAM_ATTR isInstanceOccluded(
            const Vector3 &center,
            float radius,
            const Camera &camera,
            const Viewport &viewport,
            const Matrix4x4 &viewProjMatrix,
            ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
            const DisplayConfig &cfg,
            const CullingCache &cache)
        {
            if (unlikely(!zBuffer || radius <= 0.0f))
                return false;

            const Vector3 toCenter = center - camera.position;
            const Vector3 camFwd = camera.forward();
            const float zEye = toCenter.dot(camFwd);

            if (unlikely(zEye - radius <= camera.nearPlane))
                return false;

            float rScr;
            if (likely(camera.projectionType == PERSPECTIVE))
                rScr = radius * cache.projFactor * FastMath::fastReciprocal(zEye);
            else
                rScr = radius * cache.projFactor;

            const float *m = viewProjMatrix.m;

            const float cx_w = center.x, cy_w = center.y, cz_w = center.z;
            const float clipX = m[0] * cx_w + m[4] * cy_w + m[8] * cz_w + m[12];
            const float clipY = m[1] * cx_w + m[5] * cy_w + m[9] * cz_w + m[13];
            const float clipW = m[3] * cx_w + m[7] * cy_w + m[11] * cz_w + m[15];

            const float invW = FastMath::fastReciprocal(clipW);
            const float halfW = static_cast<float>(viewport.width) * 0.5f;
            const float halfH = static_cast<float>(viewport.height) * 0.5f;

            const float cx_f = clipX * (invW * halfW) + (halfW + viewport.x);
            const float cy_f = -clipY * (invW * halfH) + (halfH + viewport.y);

            const float dCamFwdX = camFwd.x, dCamFwdY = camFwd.y, dCamFwdZ = camFwd.z;
            const float fx = cx_w - dCamFwdX * radius;
            const float fy = cy_w - dCamFwdY * radius;
            const float fz = cz_w - dCamFwdZ * radius;

            const float clipZf = m[2] * fx + m[6] * fy + m[10] * fz + m[14];
            const float clipWf = m[3] * fx + m[7] * fy + m[11] * fz + m[15];
            const float invFrontW = FastMath::fastReciprocal(clipWf);

            float objDepth = clipZf * (invFrontW * 0.5f) + 0.499f;
            if (objDepth < 0.0f)
                objDepth = 0.0f;
            const int16_t objDepthInt = static_cast<int16_t>(objDepth * 32767.0f);

            const int16_t cx = static_cast<int16_t>(cx_f);
            const int16_t cy = static_cast<int16_t>(cy_f);
            const int16_t rScrInt = static_cast<int16_t>(rScr > 1.0f ? rScr : 1.0f);

            const int16_t bandTop = currentBandOffsetY();
            const int16_t bandBottom = bandTop + static_cast<int16_t>(cfg.height);
            const int16_t cfgW = static_cast<int16_t>(cfg.width);

            const int16_t *zb = zBuffer->getBufferPtr();
            const int16_t localCy = cy - bandTop;

            const auto sampleOccluded = [&](int16_t sx, int16_t sy) -> bool
            {
                const int16_t d = zb[static_cast<size_t>(sy) * SCREEN_WIDTH + sx] & 0x7FFF;
                return (d != 0x7F7F && d < objDepthInt);
            };

            int validSamples = 0;

            const bool cyInBand = (cy >= bandTop && cy < bandBottom);
            const bool cxInScr = (cx >= 0 && cx < cfgW);

            if (cyInBand && cxInScr)
            {
                if (!sampleOccluded(cx, localCy))
                    return false;
                ++validSamples;
            }

            {
                const int16_t sxR = cx + rScrInt;
                if (cyInBand && sxR >= 0 && sxR < cfgW)
                {
                    if (!sampleOccluded(sxR, localCy))
                        return false;
                    ++validSamples;
                }
            }

            {
                const int16_t sxL = cx - rScrInt;
                if (cyInBand && sxL >= 0 && sxL < cfgW)
                {
                    if (!sampleOccluded(sxL, localCy))
                        return false;
                    ++validSamples;
                }
            }

            {
                const int16_t syD = cy + rScrInt;
                if (syD >= bandTop && syD < bandBottom && cxInScr)
                {
                    if (!sampleOccluded(cx, syD - bandTop))
                        return false;
                    ++validSamples;
                }
            }

            {
                const int16_t syU = cy - rScrInt;
                if (syU >= bandTop && syU < bandBottom && cxInScr)
                {
                    if (!sampleOccluded(cx, syU - bandTop))
                        return false;
                    ++validSamples;
                }
            }

            return (validSamples > 0);
        }

        static bool IRAM_ATTR isInstanceOccluded(
            const Vector3 &center,
            float radius,
            const Camera &camera,
            const Viewport &viewport,
            const Matrix4x4 &viewProjMatrix,
            ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
            const DisplayConfig &cfg)
        {
            static CullingCache s_cache;
            s_cache.update(camera, viewport);
            return isInstanceOccluded(center, radius, camera, viewport,
                                      viewProjMatrix, zBuffer, cfg, s_cache);
        }
    };
}
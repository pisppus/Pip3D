#pragma once

#include "Core/Platform.hpp"
#include "Core/Color.hpp"
#include "Math/Algebra.hpp"
#include "Camera/Camera.hpp"
#include "Rendering/Display/ZBuffer.hpp"
#include "Rendering/Lighting/Lighting.hpp"
#include <algorithm>

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

namespace pip3D
{
    __attribute__((always_inline)) inline void IRAM_ATTR applyDeferred3DLighting(
        uint16_t *frameBuffer,
        ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
        const Light *pointLights,
        int lightCount,
        const Camera &camera,
        const Matrix4x4 &viewProjMatrix,
        const Viewport &viewport)
    {
        if (unlikely(!frameBuffer || !zBuffer || !pointLights || lightCount <= 0))
            return;

        static constexpr int16_t kDepthMask = 0x7FFF;

        static constexpr int16_t kBayer4[4][4] = {
            {0, 512, 128, 640},
            {768, 256, 896, 384},
            {192, 704, 64, 576},
            {960, 448, 832, 320}};

        const int16_t screenW = SCREEN_WIDTH;
        const int16_t bandTop = g_bandOffsetY;
        const int16_t bandBottom = static_cast<int16_t>(bandTop + SCREEN_BAND_HEIGHT);
        const int16_t *const zbBase = zBuffer->getBufferPtr();
        if (!zbBase)
            return;

        const Vector3 camPos = camera.position;
        const Vector3 camFwd = camera.forward();
        const Vector3 camRight = camera.right();
        const Vector3 localUp = camRight.cross(camFwd);

        const float aspect = (float)viewport.width / (float)viewport.height;
        const float fovYRad = camera.fov * 0.5f * kDegToRad;
        const float tanHalfFovY = tanf(fovYRad);
        const float tanHalfFovX = tanHalfFovY * aspect;

        const float camNear = camera.nearPlane;
        const float camFar = camera.farPlane;
        const float denomFarNear = camFar - camNear;
        const float safeDenom = (denomFarNear > 1e-4f) ? denomFarNear : 1.0f;
        const float k = 32638.0f * (camFar / safeDenom);
        const float fogKVal = k;
        const float fogKnVal = k * camNear;

        const float stepU_base = 2.0f / (float)viewport.width;
        const float stepU_tanX = stepU_base * tanHalfFovX;
        const float tan2Y = tanHalfFovY * tanHalfFovY;

        for (int li = 0; li < lightCount; ++li)
        {
            const Light &L = pointLights[li];
            if (L.intensity <= 0.0f || L.range <= 0.0f)
                continue;

            const Vector3 lpos = L.position;
            const float range = L.range;
            const float rangeSq = range * range;
            const float invRangeSq = 1.0f / rangeSq;
            const float intensity = L.intensity;

            L.warmCache();
            const uint32_t lightR5 = static_cast<uint32_t>(L.cachedR * 31.0f + 0.5f);
            const uint32_t lightG6 = static_cast<uint32_t>(L.cachedG * 63.0f + 0.5f);
            const uint32_t lightB5 = static_cast<uint32_t>(L.cachedB * 31.0f + 0.5f);

            uint8_t addR_table[32];
            uint8_t addG_table[32];
            uint8_t addB_table[32];
            for (uint32_t i = 0; i < 32; ++i)
            {
                addR_table[i] = static_cast<uint8_t>((lightR5 * i) >> 5);
                addG_table[i] = static_cast<uint8_t>((lightG6 * i) >> 5);
                addB_table[i] = static_cast<uint8_t>((lightB5 * i) >> 5);
            }

            Vector3 centerScreen = CameraController::project(lpos, viewProjMatrix, viewport);

            float distToLight = (lpos - camPos).dot(camFwd);
            if (distToLight <= 0.0f)
                distToLight = 0.1f;
            float rScreen = (range * (1.0f / tanHalfFovY) / distToLight) * (viewport.height * 0.5f);

            const int originalYMin = static_cast<int>(centerScreen.y - rScreen);

            int xMin = static_cast<int>(centerScreen.x - rScreen);
            int xMax = static_cast<int>(centerScreen.x + rScreen);
            int yMin = originalYMin;
            int yMax = static_cast<int>(centerScreen.y + rScreen);

            if (xMin < 0)
                xMin = 0;
            if (xMax >= viewport.width)
                xMax = viewport.width - 1;
            if (yMin < bandTop)
                yMin = bandTop;
            if (yMax >= bandBottom)
                yMax = bandBottom - 1;

            if (xMin > xMax || yMin > yMax)
                continue;

            const float lightZMin = distToLight - range;
            const float lightZMax = distToLight + range;
            const float zMin = fmaxf(camNear, lightZMin);
            const float zMax = fminf(camFar, lightZMax);
            const int16_t dMin = static_cast<int16_t>(fogKVal - (fogKnVal / zMin));
            const int16_t dMax = static_cast<int16_t>(fogKVal - (fogKnVal / zMax));

            const Vector3 D = camPos - lpos;
            const float D2 = D.lengthSquared();
            const float D_dot_F = D.dot(camFwd);
            const float D_dot_Right = D.dot(camRight);
            const float D_dot_U = D.dot(localUp) * tanHalfFovY;

            const float u_tanX_start = (2.0f * (float)xMin / (float)viewport.width - 1.0f) * tanHalfFovX;
            const float stepD_dot_V = D_dot_Right * stepU_tanX;

            const float intensity_invRange = intensity * 32768.0f * invRangeSq;

            for (int y = yMin; y <= yMax; ++y)
            {
                const int localY = y - bandTop;
                const size_t rowOff = static_cast<size_t>(localY) * screenW;
                uint16_t *__restrict__ fbRow = frameBuffer + rowOff;
                const int16_t *__restrict__ zbRow = zbBase + rowOff;

                const float v = 1.0f - 2.0f * (float)y / (float)viewport.height;
                const float rowV2 = 1.0f + (v * v) * tan2Y;

                const float rowD_dot_V = D_dot_F + v * D_dot_U;
                float curD_dot_V = rowD_dot_V + D_dot_Right * u_tanX_start;

                const int16_t *__restrict__ zbPtr = zbRow + xMin;
                uint16_t *__restrict__ fbPtr = fbRow + xMin;
                float u_tanX = u_tanX_start;

                const int16_t *__restrict__ bayerRow = kBayer4[y & 3];

                for (int x = xMin; x <= xMax; ++x)
                {
                    const int16_t dRaw = *zbPtr;
                    const int16_t dValue = dRaw & kDepthMask;

                    if (dValue >= dMin && dValue <= dMax)
                    {
                        float denom = fogKVal - static_cast<float>(dValue);
                        denom = fmaxf(denom, 1.0f);
                        const float zEye = fogKnVal * FastMath::fastReciprocal(denom);

                        const float zEye2 = zEye * zEye;
                        const float zEye_two = 2.0f * zEye;
                        const float V2 = rowV2 + u_tanX * u_tanX;
                        const float distSq = D2 + zEye_two * curD_dot_V + zEye2 * V2;

                        if (distSq < rangeSq)
                        {
                            const float atten_scaled = intensity_invRange * (rangeSq - distSq);
                            const int16_t bayer = bayerRow[x & 3];
                            const int32_t i_fixed = static_cast<int32_t>(atten_scaled) + (bayer >> 5);
                            uint32_t i5 = static_cast<uint32_t>(i_fixed >> 10);
                            if (i5 > 31)
                                i5 = 31;

                            if (i5 != 0)
                            {
                                const uint32_t dst = *fbPtr;

                                const uint32_t addR = addR_table[i5];
                                const uint32_t addG = addG_table[i5];
                                const uint32_t addB = addB_table[i5];

                                const Color dstCol(dst);
                                const uint32_t r = std::min(static_cast<uint32_t>(dstCol.r5()) + addR, 31u);
                                const uint32_t g = std::min(static_cast<uint32_t>(dstCol.g6()) + addG, 63u);
                                const uint32_t b = std::min(static_cast<uint32_t>(dstCol.b5()) + addB, 31u);

                                *fbPtr = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                            }
                        }
                    }
                    curD_dot_V += stepD_dot_V;
                    u_tanX += stepU_tanX;
                    ++zbPtr;
                    ++fbPtr;
                }
            }
        }
    }
}
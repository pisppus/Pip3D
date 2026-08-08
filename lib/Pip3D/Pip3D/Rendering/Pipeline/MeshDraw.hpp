#pragma once

#include "Core/Platform.hpp"
#include "Camera/Camera.hpp"
#include "Math/Algebra.hpp"

#include "Rendering/Buffers/ZBuffer.hpp"
#include "Rendering/Buffers/FrameBuffer.hpp"
#include "Rendering/Lighting/Lighting.hpp"
#include "Rendering/Pipeline/Rasterizer/Common.hpp"
#include "Rendering/Pipeline/Rasterizer/Solid.hpp"
#include "Rendering/Pipeline/Rasterizer/Textured.hpp"

#include "Shading.hpp"

namespace pip3D
{
    namespace MeshRenderer
    {

        struct ClipVert
        {
            Vector3 pos;
            float u, v;
            float d;
            float lr, lg, lb;
        };

        PIP3D_FORCE_INLINE static ClipVert lerpClipVert(const ClipVert &a,
                                                        const ClipVert &b,
                                                        float t) noexcept
        {
            const float it = 1.0f - t;
            ClipVert r;
            r.pos.x = a.pos.x * it + b.pos.x * t;
            r.pos.y = a.pos.y * it + b.pos.y * t;
            r.pos.z = a.pos.z * it + b.pos.z * t;
            r.u = a.u * it + b.u * t;
            r.v = a.v * it + b.v * t;
            r.d = a.d * it + b.d * t;
            r.lr = a.lr * it + b.lr * t;
            r.lg = a.lg * it + b.lg * t;
            r.lb = a.lb * it + b.lb * t;
            return r;
        }

        PIP3D_FORCE_INLINE static void decodeColorToFloat(uint16_t color,
                                                          float &baseR,
                                                          float &baseG,
                                                          float &baseB) noexcept
        {
            Color(color).toFloat(baseR, baseG, baseB);
        }

        PIP3D_HOT static void drawTriangle3D_Color_Preprojected(
            const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
            const Vector3 &p0, const Vector3 &p1, const Vector3 &p2,
            float baseR, float baseG, float baseB,
            const Vector3 &camPos,
            float viewportHalfWidth, float viewportHalfHeight,
            float viewportWidth,
            int16_t bandTop, int16_t bandBottom, float bandTopF,
            FrameBuffer &framebuffer,
            ZBuffer *zBuffer,
            const Light *lights,
            int activeLightCount,
            bool useUniformColor,
            uint16_t uniformColor)
        {
            uint16_t *const frameBuffer = framebuffer.getBuffer();
            const DisplayConfig &framebufferConfig = framebuffer.getConfig();

            const float minY = (p0.y < p1.y) ? ((p0.y < p2.y) ? p0.y : p2.y)
                                             : ((p1.y < p2.y) ? p1.y : p2.y);
            const float maxY = (p0.y > p1.y) ? ((p0.y > p2.y) ? p0.y : p2.y)
                                             : ((p1.y > p2.y) ? p1.y : p2.y);
            if (maxY < bandTop || minY >= bandBottom)
                return;

            const float minX = (p0.x < p1.x) ? ((p0.x < p2.x) ? p0.x : p2.x)
                                             : ((p1.x < p2.x) ? p1.x : p2.x);
            const float maxX = (p0.x > p1.x) ? ((p0.x > p2.x) ? p0.x : p2.x)
                                             : ((p1.x > p2.x) ? p1.x : p2.x);
            if (maxX < 0.0f || minX >= viewportWidth)
                return;

            uint16_t shadedColor = uniformColor;

            if (!useUniformColor)
            {

                const float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
                const float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
                const float nx = e1y * e2z - e1z * e2y;
                const float ny = e1z * e2x - e1x * e2z;
                const float nz = e1x * e2y - e1y * e2x;

                const float nLenSq = nx * nx + ny * ny + nz * nz;
                const float nInvLen = (nLenSq > 1e-12f) ? FastMath::fastInvSqrt(nLenSq) : 0.0f;
                const Vector3 normal(nx * nInvLen, ny * nInvLen, nz * nInvLen);

                const float dx = camPos.x - v0.x;
                const float dy = camPos.y - v0.y;
                const float dz = camPos.z - v0.z;
                const float lenSq = dx * dx + dy * dy + dz * dz;
                const float invLen = (lenSq > 1e-8f) ? FastMath::fastInvSqrt(lenSq) : 0.0f;
                const Vector3 viewDir(dx * invLen, dy * invLen, dz * invLen);

                float finalR, finalG, finalB;
                Shading::calculateLighting(v0, normal, viewDir,
                                           lights, activeLightCount,
                                           baseR, baseG, baseB,
                                           finalR, finalG, finalB);

                const auto &fog = Rasterizer::g_fogState;
                if (fog.enabled)
                {
                    const float dist = lenSq * invLen;
                    float fogFactor = (dist - fog.worldNear) * fog.worldScale;
                    fogFactor = clamp(fogFactor, 0.0f, 1.0f);
                    const float invFog = 1.0f - fogFactor;
                    finalR = finalR * invFog + fog.color_r * fogFactor;
                    finalG = finalG * invFog + fog.color_g_f * fogFactor;
                    finalB = finalB * invFog + fog.color_b_f * fogFactor;
                }

                shadedColor = Color::fromFloat(finalR, finalG, finalB).rgb565;
            }

            Rasterizer::fillTriangle(p0.x, p0.y - bandTopF, p0.z,
                                     p1.x, p1.y - bandTopF, p1.z,
                                     p2.x, p2.y - bandTopF, p2.z,
                                     shadedColor,
                                     frameBuffer,
                                     zBuffer,
                                     framebufferConfig);
        }

        PIP3D_HOT static void clipAndDrawNearTextured(const ClipVert inVerts[3],
                                                      float nearD,
                                                      const Camera &camera,
                                                      const Viewport &viewport,
                                                      const Matrix4x4 &viewProjMatrix,
                                                      FrameBuffer &framebuffer,
                                                      ZBuffer *zBuffer,
                                                      const Texture &tex)
        {
            ClipVert clipped[4];
            int outCount = 0;

#define PIP3D_CLIP_EDGE(IDX_A, IDX_B)                                \
    {                                                                \
        const ClipVert &P0 = inVerts[IDX_A];                         \
        const ClipVert &P1 = inVerts[IDX_B];                         \
        const float d0 = P0.d;                                       \
        const float d1 = P1.d;                                       \
        const bool in0 = d0 >= nearD;                                \
        const bool in1 = d1 >= nearD;                                \
        if (in0 && in1)                                              \
            clipped[outCount++] = P1;                                \
        else if (in0 != in1)                                         \
        {                                                            \
            const float denom = d1 - d0;                             \
            float t = (fabsf(denom) < 1e-6f) ? 0.0f                  \
                                             : (nearD - d0) / denom; \
            t = clamp(t, 0.0f, 1.0f);                                \
            const ClipVert ip = lerpClipVert(P0, P1, t);             \
            if (in0)                                                 \
                clipped[outCount++] = ip;                            \
            else                                                     \
            {                                                        \
                clipped[outCount++] = ip;                            \
                clipped[outCount++] = P1;                            \
            }                                                        \
        }                                                            \
    }
            PIP3D_CLIP_EDGE(0, 1);
            PIP3D_CLIP_EDGE(1, 2);
            PIP3D_CLIP_EDGE(2, 0);
#undef PIP3D_CLIP_EDGE

            if (outCount < 3)
                return;

            const float viewportHalfWidth = static_cast<float>(viewport.width) * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
            const DisplayConfig &framebufferConfig = framebuffer.getConfig();
            const float viewportWidth = static_cast<float>(viewport.width);
            const float bandTopF = static_cast<float>(bandTop);

            Vector3 proj[4];
            for (int i = 0; i < outCount; ++i)
                proj[i] = CameraController::project(clipped[i].pos, viewProjMatrix,
                                                    viewportHalfWidth, viewportHalfHeight,
                                                    viewport.x, viewport.y);

            auto drawTri = [&](int a, int b, int c)
            {
                const Vector3 &p0 = proj[a];
                const Vector3 &p1 = proj[b];
                const Vector3 &p2 = proj[c];

                const float minY = (p0.y < p1.y) ? ((p0.y < p2.y) ? p0.y : p2.y)
                                                 : ((p1.y < p2.y) ? p1.y : p2.y);
                const float maxY = (p0.y > p1.y) ? ((p0.y > p2.y) ? p0.y : p2.y)
                                                 : ((p1.y > p2.y) ? p1.y : p2.y);
                if (maxY < bandTop || minY >= bandBottom)
                    return;

                const float minX = (p0.x < p1.x) ? ((p0.x < p2.x) ? p0.x : p2.x)
                                                 : ((p1.x < p2.x) ? p1.x : p2.x);
                const float maxX = (p0.x > p1.x) ? ((p0.x > p2.x) ? p0.x : p2.x)
                                                 : ((p1.x > p2.x) ? p1.x : p2.x);
                if (maxX < 0.0f || minX >= viewportWidth)
                    return;

                Rasterizer::fillTriangleTextured(
                    p0.x, p0.y - bandTopF, p0.z,
                    p1.x, p1.y - bandTopF, p1.z,
                    p2.x, p2.y - bandTopF, p2.z,
                    clipped[a].u, clipped[a].v,
                    clipped[b].u, clipped[b].v,
                    clipped[c].u, clipped[c].v,
                    clipped[a].d, clipped[b].d, clipped[c].d,
                    clipped[a].lr, clipped[a].lg, clipped[a].lb,
                    clipped[b].lr, clipped[b].lg, clipped[b].lb,
                    clipped[c].lr, clipped[c].lg, clipped[c].lb,
                    tex,
                    framebuffer.getBuffer(),
                    zBuffer,
                    framebufferConfig);
            };

            drawTri(0, 1, 2);
            if (outCount == 4)
                drawTri(0, 2, 3);
        }

        PIP3D_HOT static void clipAndDrawNear(
            const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
            float d0, float d1, float d2,
            float nearD,
            float baseR, float baseG, float baseB,
            const Vector3 &camPos,
            float viewportHalfWidth, float viewportHalfHeight,
            float viewportWidth,
            int16_t bandTop, int16_t bandBottom, float bandTopF,
            const Matrix4x4 &viewProjMatrix,
            FrameBuffer &framebuffer,
            ZBuffer *zBuffer,
            const Light *lights,
            int activeLightCount,
            bool useUniformColor,
            uint16_t uniformColor)
        {
            Vector3 clipped[4];
            int outCount = 0;

#define PIP3D_CLIP_EDGE_V(IDX_A, IDX_B)                              \
    {                                                                \
        const Vector3 &P0 = (IDX_A == 0) ? v0 : (IDX_A == 1) ? v1    \
                                                             : v2;   \
        const Vector3 &P1 = (IDX_B == 0) ? v0 : (IDX_B == 1) ? v1    \
                                                             : v2;   \
        const float da = (IDX_A == 0) ? d0 : (IDX_A == 1) ? d1       \
                                                          : d2;      \
        const float db = (IDX_B == 0) ? d0 : (IDX_B == 1) ? d1       \
                                                          : d2;      \
        const bool in0 = da >= nearD;                                \
        const bool in1 = db >= nearD;                                \
        if (in0 && in1)                                              \
            clipped[outCount++] = P1;                                \
        else if (in0 != in1)                                         \
        {                                                            \
            const float denom = db - da;                             \
            float t = (fabsf(denom) < 1e-6f) ? 0.0f                  \
                                             : (nearD - da) / denom; \
            t = clamp(t, 0.0f, 1.0f);                                \
            const Vector3 ip = P0 + (P1 - P0) * t;                   \
            if (in0)                                                 \
                clipped[outCount++] = ip;                            \
            else                                                     \
            {                                                        \
                clipped[outCount++] = ip;                            \
                clipped[outCount++] = P1;                            \
            }                                                        \
        }                                                            \
    }
            PIP3D_CLIP_EDGE_V(0, 1);
            PIP3D_CLIP_EDGE_V(1, 2);
            PIP3D_CLIP_EDGE_V(2, 0);
#undef PIP3D_CLIP_EDGE_V

            if (outCount < 3)
                return;

            const Vector3 p0 = CameraController::project(clipped[0], viewProjMatrix,
                                                         viewportHalfWidth, viewportHalfHeight,
                                                         0, 0);
            const Vector3 p1 = CameraController::project(clipped[1], viewProjMatrix,
                                                         viewportHalfWidth, viewportHalfHeight,
                                                         0, 0);
            const Vector3 p2 = CameraController::project(clipped[2], viewProjMatrix,
                                                         viewportHalfWidth, viewportHalfHeight,
                                                         0, 0);

            drawTriangle3D_Color_Preprojected(clipped[0], clipped[1], clipped[2],
                                              p0, p1, p2,
                                              baseR, baseG, baseB,
                                              camPos,
                                              viewportHalfWidth, viewportHalfHeight,
                                              viewportWidth,
                                              bandTop, bandBottom, bandTopF,
                                              framebuffer, zBuffer,
                                              lights, activeLightCount,
                                              useUniformColor,
                                              uniformColor);

            if (outCount == 4)
            {
                const Vector3 p3 = CameraController::project(clipped[3], viewProjMatrix,
                                                             viewportHalfWidth, viewportHalfHeight,
                                                             0, 0);
                drawTriangle3D_Color_Preprojected(clipped[0], clipped[2], clipped[3],
                                                  p0, p2, p3,
                                                  baseR, baseG, baseB,
                                                  camPos,
                                                  viewportHalfWidth, viewportHalfHeight,
                                                  viewportWidth,
                                                  bandTop, bandBottom, bandTopF,
                                                  framebuffer, zBuffer,
                                                  lights, activeLightCount,
                                                  useUniformColor,
                                                  uniformColor);
            }
        }

        PIP3D_HOT static void drawTriangle3D_Preprojected(
            const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
            const Vector3 &p0, const Vector3 &p1, const Vector3 &p2,
            float d0, float d1, float d2,
            bool partiallyClipped,
            float nearD,
            const Vector3 &camPos,
            uint16_t color,
            const Matrix4x4 &viewProjMatrix,
            float viewportHalfWidth, float viewportHalfHeight, float viewportWidth,
            int16_t bandTop, int16_t bandBottom, float bandTopF,
            FrameBuffer &framebuffer,
            ZBuffer *zBuffer,
            const Light *lights,
            int activeLightCount,
            bool useUniformColor = false,
            uint16_t uniformColor = 0)
        {
            float baseR = 0.0f, baseG = 0.0f, baseB = 0.0f;
            if (!useUniformColor)
                Color(color).toFloat(baseR, baseG, baseB);

            if (!partiallyClipped)
            {
                drawTriangle3D_Color_Preprojected(v0, v1, v2,
                                                  p0, p1, p2,
                                                  baseR, baseG, baseB,
                                                  camPos,
                                                  viewportHalfWidth, viewportHalfHeight,
                                                  viewportWidth,
                                                  bandTop, bandBottom, bandTopF,
                                                  framebuffer, zBuffer,
                                                  lights, activeLightCount,
                                                  useUniformColor,
                                                  uniformColor);
                return;
            }

            clipAndDrawNear(v0, v1, v2, d0, d1, d2, nearD,
                            baseR, baseG, baseB,
                            camPos,
                            viewportHalfWidth, viewportHalfHeight,
                            viewportWidth,
                            bandTop, bandBottom, bandTopF,
                            viewProjMatrix,
                            framebuffer, zBuffer,
                            lights, activeLightCount,
                            useUniformColor,
                            uniformColor);
        }

        PIP3D_HOT static void drawTriangle3D(
            const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
            uint16_t color,
            const Camera &camera,
            const Viewport &viewport,
            const Matrix4x4 &viewProjMatrix,
            FrameBuffer &framebuffer,
            ZBuffer *zBuffer,
            const Light *lights,
            int activeLightCount,
            bool useUniformColor = false,
            uint16_t uniformColor = 0)
        {
            float baseR = 0.0f, baseG = 0.0f, baseB = 0.0f;
            if (!useUniformColor)
                Color(color).toFloat(baseR, baseG, baseB);

            const Vector3 &camPos = camera.position;
            const Vector3 &camFwd = camera.forward();
            const float nearD = camera.nearPlane;

            const float d0 = (v0.x - camPos.x) * camFwd.x + (v0.y - camPos.y) * camFwd.y + (v0.z - camPos.z) * camFwd.z;
            const float d1 = (v1.x - camPos.x) * camFwd.x + (v1.y - camPos.y) * camFwd.y + (v1.z - camPos.z) * camFwd.z;
            const float d2 = (v2.x - camPos.x) * camFwd.x + (v2.y - camPos.y) * camFwd.y + (v2.z - camPos.z) * camFwd.z;

            const uint8_t mask = (uint8_t)((d0 < nearD) | ((d1 < nearD) << 1) | ((d2 < nearD) << 2));
            if (mask == 0b111)
                return;

            const float viewportHalfWidth = static_cast<float>(viewport.width) * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
            const float viewportWidth = static_cast<float>(viewport.width);
            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
            const float bandTopF = static_cast<float>(bandTop);

            if (mask == 0b000)
            {
                const Vector3 p0 = CameraController::project(v0, viewProjMatrix,
                                                             viewportHalfWidth, viewportHalfHeight, 0, 0);
                const Vector3 p1 = CameraController::project(v1, viewProjMatrix,
                                                             viewportHalfWidth, viewportHalfHeight, 0, 0);
                const Vector3 p2 = CameraController::project(v2, viewProjMatrix,
                                                             viewportHalfWidth, viewportHalfHeight, 0, 0);
                drawTriangle3D_Color_Preprojected(v0, v1, v2,
                                                  p0, p1, p2,
                                                  baseR, baseG, baseB,
                                                  camPos,
                                                  viewportHalfWidth, viewportHalfHeight,
                                                  viewportWidth,
                                                  bandTop, bandBottom, bandTopF,
                                                  framebuffer, zBuffer,
                                                  lights, activeLightCount,
                                                  useUniformColor,
                                                  uniformColor);
                return;
            }

            clipAndDrawNear(v0, v1, v2, d0, d1, d2, nearD,
                            baseR, baseG, baseB,
                            camPos,
                            viewportHalfWidth, viewportHalfHeight,
                            viewportWidth,
                            bandTop, bandBottom, bandTopF,
                            viewProjMatrix,
                            framebuffer, zBuffer,
                            lights, activeLightCount,
                            useUniformColor,
                            uniformColor);
        }
    }
}
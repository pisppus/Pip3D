#pragma once

#include "Camera/Camera.hpp"
#include "Camera/Frustum.hpp"
#include "Math/Algebra.hpp"
#include "Geometry/Mesh.hpp"
#include "Rendering/Display/ZBuffer.hpp"
#include "Rendering/Display/FrameBuffer.hpp"
#include "Rendering/Lighting/Lighting.hpp"
#include "Rasterizer.hpp"
#include "Shading.hpp"

namespace pip3D
{

    class MeshRenderer
    {
    public:
        static constexpr float RGB565_RED_TO_FLOAT = 1.0f / 31.0f;
        static constexpr float RGB565_GREEN_TO_FLOAT = 1.0f / 63.0f;
        static constexpr float RGB565_BLUE_TO_FLOAT = 1.0f / 31.0f;

        static void decodeColorToFloat(uint16_t color,
                                       float &baseR,
                                       float &baseG,
                                       float &baseB)
        {
            Color(color).toFloat(baseR, baseG, baseB);
        }

        static void drawTriangle3D_Preprojected(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
                                                const Vector3 &p0, const Vector3 &p1, const Vector3 &p2,
                                                uint16_t color,
                                                const Camera &camera,
                                                const Viewport &viewport,
                                                const Matrix4x4 &viewProjMatrix,
                                                FrameBuffer &framebuffer,
                                                ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                                const Light *lights,
                                                int activeLightCount,
                                                bool backfaceCullingEnabled,
                                                uint32_t &statsTrianglesTotal,
                                                uint32_t &statsTrianglesBackfaceCulled,
                                                bool useUniformColor = false,
                                                uint16_t uniformColor = 0)
        {
            float baseR;
            float baseG;
            float baseB;
            decodeColorToFloat(color, baseR, baseG, baseB);

            drawTriangle3D_Clipped_Preprojected(v0, v1, v2,
                                                p0, p1, p2,
                                                baseR, baseG, baseB,
                                                camera, viewport, viewProjMatrix,
                                                framebuffer, zBuffer,
                                                lights, activeLightCount,
                                                backfaceCullingEnabled,
                                                statsTrianglesTotal,
                                                statsTrianglesBackfaceCulled,
                                                useUniformColor,
                                                uniformColor);
        }

        static void drawTriangle3D(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
                                   uint16_t color,
                                   const Camera &camera,
                                   const Viewport &viewport,
                                   const Matrix4x4 &viewProjMatrix,
                                   FrameBuffer &framebuffer,
                                   ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                   const Light *lights,
                                   int activeLightCount,
                                   bool backfaceCullingEnabled,
                                   uint32_t &statsTrianglesTotal,
                                   uint32_t &statsTrianglesBackfaceCulled,
                                   bool useUniformColor = false,
                                   uint16_t uniformColor = 0)
        {
            float baseR;
            float baseG;
            float baseB;
            decodeColorToFloat(color, baseR, baseG, baseB);

            drawTriangle3D_Clipped(v0, v1, v2,
                                   baseR, baseG, baseB,
                                   camera, viewport, viewProjMatrix,
                                   framebuffer, zBuffer,
                                   lights, activeLightCount,
                                   backfaceCullingEnabled,
                                   statsTrianglesTotal,
                                   statsTrianglesBackfaceCulled,
                                   useUniformColor,
                                   uniformColor);
        }

    private:
        static void drawTriangle3D_Color_Preprojected(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
                                                      const Vector3 &p0, const Vector3 &p1, const Vector3 &p2,
                                                      float baseR,
                                                      float baseG,
                                                      float baseB,
                                                      const Camera &camera,
                                                      const Viewport &viewport,
                                                      FrameBuffer &framebuffer,
                                                      ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                                      const Light *lights,
                                                      int activeLightCount,
                                                      bool useUniformColor,
                                                      uint16_t uniformColor)
        {
            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
            const float viewportWidth = static_cast<float>(viewport.width);
            uint16_t *const frameBuffer = framebuffer.getBuffer();
            const DisplayConfig &framebufferConfig = framebuffer.getConfig();

            float minY = fminf(p0.y, fminf(p1.y, p2.y));
            float maxY = fmaxf(p0.y, fmaxf(p1.y, p2.y));
            if (maxY < bandTop || minY >= bandBottom)
                return;

            float minX = fminf(p0.x, fminf(p1.x, p2.x));
            float maxX = fmaxf(p0.x, fmaxf(p1.x, p2.x));
            if (maxX < 0.0f || minX >= viewportWidth)
                return;

            Vector3 lp0 = p0;
            Vector3 lp1 = p1;
            Vector3 lp2 = p2;
            lp0.y -= (float)bandTop;
            lp1.y -= (float)bandTop;
            lp2.y -= (float)bandTop;

            uint16_t shadedColor = uniformColor;

            if (!useUniformColor)
            {
                Vector3 edge1 = v1 - v0;
                Vector3 edge2 = v2 - v0;
                Vector3 normal = edge1.cross(edge2);
                normal.normalize();

                const float dx = camera.position.x - v0.x;
                const float dy = camera.position.y - v0.y;
                const float dz = camera.position.z - v0.z;
                const float lenSq = dx * dx + dy * dy + dz * dz;
                const float invLen = (lenSq > 1e-8f) ? FastMath::fastInvSqrt(lenSq) : 0.0f;
                const Vector3 viewDir(dx * invLen, dy * invLen, dz * invLen);

                float finalR, finalG, finalB;
                Shading::calculateLighting(v0, normal, viewDir,
                                           lights, activeLightCount,
                                           baseR, baseG, baseB,
                                           finalR, finalG, finalB);

                if (Rasterizer::g_fogState.enabled)
                {
                    float dist = lenSq * invLen;
                    float fogFactor = (dist - Rasterizer::g_fogState.worldNear) * Rasterizer::g_fogState.worldScale;
                    if (fogFactor < 0.0f)
                        fogFactor = 0.0f;
                    if (fogFactor > 1.0f)
                        fogFactor = 1.0f;

                    finalR = finalR * (1.0f - fogFactor) + Rasterizer::g_fogState.color_r * fogFactor;
                    finalG = finalG * (1.0f - fogFactor) + Rasterizer::g_fogState.color_g_f * fogFactor;
                    finalB = finalB * (1.0f - fogFactor) + Rasterizer::g_fogState.color_b_f * fogFactor;
                }

                shadedColor = Color::fromFloat(finalR, finalG, finalB).rgb565;
            }

            Rasterizer::fillTriangle(lp0.x, lp0.y, lp0.z,
                                     lp1.x, lp1.y, lp1.z,
                                     lp2.x, lp2.y, lp2.z,
                                     shadedColor,
                                     frameBuffer,
                                     zBuffer,
                                     framebufferConfig);
        }

        struct ClipVert
        {
            Vector3 pos;
            float u, v;
            float d;
            float lr, lg, lb;
        };

        static inline ClipVert lerpClipVert(const ClipVert &a,
                                            const ClipVert &b,
                                            float t)
        {
            ClipVert r;
            r.pos = a.pos + (b.pos - a.pos) * t;
            r.u = a.u + (b.u - a.u) * t;
            r.v = a.v + (b.v - a.v) * t;
            r.d = a.d + (b.d - a.d) * t;
            r.lr = a.lr + (b.lr - a.lr) * t;
            r.lg = a.lg + (b.lg - a.lg) * t;
            r.lb = a.lb + (b.lb - a.lb) * t;
            return r;
        }

        static void clipAndDrawNearTextured(const ClipVert inVerts[3],
                                            float nearD,
                                            const Camera &camera,
                                            const Viewport &viewport,
                                            const Matrix4x4 &viewProjMatrix,
                                            FrameBuffer &framebuffer,
                                            ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                            const Texture &tex)
        {
            ClipVert clipped[4];
            int outCount = 0;

            for (int i = 0; i < 3; ++i)
            {
                int j = (i + 1) % 3;
                const ClipVert &P0 = inVerts[i];
                const ClipVert &P1 = inVerts[j];
                const float d0 = P0.d;
                const float d1 = P1.d;
                const bool in0 = d0 >= nearD;
                const bool in1 = d1 >= nearD;

                if (in0 && in1)
                {
                    clipped[outCount++] = P1;
                }
                else if (in0 != in1)
                {
                    float denom = d1 - d0;
                    float t = (fabsf(denom) < 1e-6f) ? 0.0f : (nearD - d0) / denom;
                    if (t < 0.0f)
                        t = 0.0f;
                    if (t > 1.0f)
                        t = 1.0f;

                    ClipVert ip = lerpClipVert(P0, P1, t);

                    if (in0)
                    {
                        clipped[outCount++] = ip;
                    }
                    else
                    {
                        clipped[outCount++] = ip;
                        clipped[outCount++] = P1;
                    }
                }
            }

            if (outCount < 3)
                return;

            const float viewportHalfWidth = static_cast<float>(viewport.width) * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
            const DisplayConfig &framebufferConfig = framebuffer.getConfig();
            const float viewportWidth = static_cast<float>(viewport.width);

            auto projectAndDraw = [&](const ClipVert &cv0,
                                      const ClipVert &cv1,
                                      const ClipVert &cv2)
            {
                Vector3 p0 = CameraController::project(cv0.pos, viewProjMatrix,
                                                       viewportHalfWidth, viewportHalfHeight,
                                                       viewport.x, viewport.y);
                Vector3 p1 = CameraController::project(cv1.pos, viewProjMatrix,
                                                       viewportHalfWidth, viewportHalfHeight,
                                                       viewport.x, viewport.y);
                Vector3 p2 = CameraController::project(cv2.pos, viewProjMatrix,
                                                       viewportHalfWidth, viewportHalfHeight,
                                                       viewport.x, viewport.y);

                float minY = fminf(p0.y, fminf(p1.y, p2.y));
                float maxY = fmaxf(p0.y, fmaxf(p1.y, p2.y));
                if (maxY < bandTop || minY >= bandBottom)
                    return;

                float minX = fminf(p0.x, fminf(p1.x, p2.x));
                float maxX = fmaxf(p0.x, fmaxf(p1.x, p2.x));
                if (maxX < 0.0f || minX >= viewportWidth)
                    return;

                p0.y -= (float)bandTop;
                p1.y -= (float)bandTop;
                p2.y -= (float)bandTop;

                Rasterizer::fillTriangleTextured(
                    p0.x, p0.y, p0.z,
                    p1.x, p1.y, p1.z,
                    p2.x, p2.y, p2.z,
                    cv0.u, cv0.v,
                    cv1.u, cv1.v,
                    cv2.u, cv2.v,
                    cv0.d, cv1.d, cv2.d,
                    cv0.lr, cv0.lg, cv0.lb,
                    cv1.lr, cv1.lg, cv1.lb,
                    cv2.lr, cv2.lg, cv2.lb,
                    tex,
                    framebuffer.getBuffer(),
                    zBuffer,
                    framebufferConfig);
            };

            projectAndDraw(clipped[0], clipped[1], clipped[2]);
            if (outCount == 4)
                projectAndDraw(clipped[0], clipped[2], clipped[3]);
        }

        static void clipAndDrawNear(const Vector3 inVerts[3],
                                    const float dist[3],
                                    float nearD,
                                    float baseR,
                                    float baseG,
                                    float baseB,
                                    const Camera &camera,
                                    const Viewport &viewport,
                                    const Matrix4x4 &viewProjMatrix,
                                    FrameBuffer &framebuffer,
                                    ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                    const Light *lights,
                                    int activeLightCount,
                                    bool useUniformColor,
                                    uint16_t uniformColor)
        {
            Vector3 clipped[4];
            int outCount = 0;

            for (int i = 0; i < 3; ++i)
            {
                int j = (i + 1) % 3;
                bool in0 = dist[i] >= nearD;
                bool in1 = dist[j] >= nearD;
                const Vector3 &P0 = inVerts[i];
                const Vector3 &P1 = inVerts[j];
                float d0 = dist[i];
                float d1 = dist[j];

                if (in0 && in1)
                {
                    clipped[outCount++] = P1;
                }
                else if (in0 != in1)
                {
                    float denom = d1 - d0;
                    float t = (fabsf(denom) < 1e-6f) ? 0.0f : (nearD - d0) / denom;
                    if (t < 0.0f)
                        t = 0.0f;
                    if (t > 1.0f)
                        t = 1.0f;
                    Vector3 ip = P0 + (P1 - P0) * t;

                    if (in0)
                        clipped[outCount++] = ip;
                    else
                    {
                        clipped[outCount++] = ip;
                        clipped[outCount++] = P1;
                    }
                }
            }

            if (outCount < 3)
                return;

            const float viewportHalfWidth = static_cast<float>(viewport.width) * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;

            Vector3 p0 = CameraController::project(clipped[0], viewProjMatrix,
                                                   viewportHalfWidth, viewportHalfHeight,
                                                   viewport.x, viewport.y);
            Vector3 p1 = CameraController::project(clipped[1], viewProjMatrix,
                                                   viewportHalfWidth, viewportHalfHeight,
                                                   viewport.x, viewport.y);
            Vector3 p2 = CameraController::project(clipped[2], viewProjMatrix,
                                                   viewportHalfWidth, viewportHalfHeight,
                                                   viewport.x, viewport.y);

            drawTriangle3D_Color_Preprojected(clipped[0], clipped[1], clipped[2],
                                              p0, p1, p2,
                                              baseR, baseG, baseB,
                                              camera, viewport,
                                              framebuffer, zBuffer,
                                              lights, activeLightCount,
                                              useUniformColor,
                                              uniformColor);

            if (outCount == 4)
            {
                Vector3 p3 = CameraController::project(clipped[3], viewProjMatrix,
                                                       viewportHalfWidth, viewportHalfHeight,
                                                       viewport.x, viewport.y);

                drawTriangle3D_Color_Preprojected(clipped[0], clipped[2], clipped[3],
                                                  p0, p2, p3,
                                                  baseR, baseG, baseB,
                                                  camera, viewport,
                                                  framebuffer, zBuffer,
                                                  lights, activeLightCount,
                                                  useUniformColor,
                                                  uniformColor);
            }
        }

        static void drawTriangle3D_Clipped_Preprojected(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
                                                        const Vector3 &p0, const Vector3 &p1, const Vector3 &p2,
                                                        float baseR,
                                                        float baseG,
                                                        float baseB,
                                                        const Camera &camera,
                                                        const Viewport &viewport,
                                                        const Matrix4x4 &viewProjMatrix,
                                                        FrameBuffer &framebuffer,
                                                        ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                                        const Light *lights,
                                                        int activeLightCount,
                                                        bool backfaceCullingEnabled,
                                                        uint32_t &statsTrianglesTotal,
                                                        uint32_t &statsTrianglesBackfaceCulled,
                                                        bool useUniformColor = false,
                                                        uint16_t uniformColor = 0)
        {
            (void)backfaceCullingEnabled;
            (void)statsTrianglesTotal;
            (void)statsTrianglesBackfaceCulled;

            if (camera.projectionType == PERSPECTIVE)
            {
                const Vector3 camPos = camera.position;
                const Vector3 camFwd = camera.forward();
                const float nearD = camera.nearPlane;

                float d0 = (v0 - camPos).dot(camFwd);
                float d1 = (v1 - camPos).dot(camFwd);
                float d2 = (v2 - camPos).dot(camFwd);

                if (d0 < nearD && d1 < nearD && d2 < nearD)
                    return;

                if (d0 >= nearD && d1 >= nearD && d2 >= nearD)
                {
                    drawTriangle3D_Color_Preprojected(v0, v1, v2,
                                                      p0, p1, p2,
                                                      baseR, baseG, baseB,
                                                      camera, viewport,
                                                      framebuffer, zBuffer,
                                                      lights, activeLightCount,
                                                      useUniformColor,
                                                      uniformColor);
                    return;
                }

                const Vector3 inVerts[3] = {v0, v1, v2};
                const float dist[3] = {d0, d1, d2};
                clipAndDrawNear(inVerts, dist, nearD,
                                baseR, baseG, baseB,
                                camera, viewport, viewProjMatrix,
                                framebuffer, zBuffer,
                                lights, activeLightCount,
                                useUniformColor,
                                uniformColor);
                return;
            }

            drawTriangle3D_Color_Preprojected(v0, v1, v2,
                                              p0, p1, p2,
                                              baseR, baseG, baseB,
                                              camera, viewport,
                                              framebuffer, zBuffer,
                                              lights, activeLightCount,
                                              useUniformColor,
                                              uniformColor);
        }

        static void drawTriangle3D_Clipped(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
                                           float baseR,
                                           float baseG,
                                           float baseB,
                                           const Camera &camera,
                                           const Viewport &viewport,
                                           const Matrix4x4 &viewProjMatrix,
                                           FrameBuffer &framebuffer,
                                           ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                           const Light *lights,
                                           int activeLightCount,
                                           bool backfaceCullingEnabled,
                                           uint32_t &statsTrianglesTotal,
                                           uint32_t &statsTrianglesBackfaceCulled,
                                           bool useUniformColor = false,
                                           uint16_t uniformColor = 0)
        {
            (void)backfaceCullingEnabled;
            (void)statsTrianglesTotal;
            (void)statsTrianglesBackfaceCulled;

            if (camera.projectionType == PERSPECTIVE)
            {
                const Vector3 camPos = camera.position;
                const Vector3 camFwd = camera.forward();
                const float nearD = camera.nearPlane;

                float d0 = (v0 - camPos).dot(camFwd);
                float d1 = (v1 - camPos).dot(camFwd);
                float d2 = (v2 - camPos).dot(camFwd);

                if (d0 < nearD && d1 < nearD && d2 < nearD)
                    return;

                if (d0 >= nearD && d1 >= nearD && d2 >= nearD)
                {
                    const float viewportHalfWidth = static_cast<float>(viewport.width) * 0.5f;
                    const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
                    Vector3 p0 = CameraController::project(v0, viewProjMatrix,
                                                           viewportHalfWidth, viewportHalfHeight,
                                                           viewport.x, viewport.y);
                    Vector3 p1 = CameraController::project(v1, viewProjMatrix,
                                                           viewportHalfWidth, viewportHalfHeight,
                                                           viewport.x, viewport.y);
                    Vector3 p2 = CameraController::project(v2, viewProjMatrix,
                                                           viewportHalfWidth, viewportHalfHeight,
                                                           viewport.x, viewport.y);
                    drawTriangle3D_Color_Preprojected(v0, v1, v2,
                                                      p0, p1, p2,
                                                      baseR, baseG, baseB,
                                                      camera, viewport,
                                                      framebuffer, zBuffer,
                                                      lights, activeLightCount,
                                                      useUniformColor,
                                                      uniformColor);
                    return;
                }

                const Vector3 inVerts[3] = {v0, v1, v2};
                const float dist[3] = {d0, d1, d2};
                clipAndDrawNear(inVerts, dist, nearD,
                                baseR, baseG, baseB,
                                camera, viewport, viewProjMatrix,
                                framebuffer, zBuffer,
                                lights, activeLightCount,
                                useUniformColor,
                                uniformColor);
                return;
            }

            const float viewportHalfWidth = static_cast<float>(viewport.width) * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
            Vector3 p0 = CameraController::project(v0, viewProjMatrix,
                                                   viewportHalfWidth, viewportHalfHeight,
                                                   viewport.x, viewport.y);
            Vector3 p1 = CameraController::project(v1, viewProjMatrix,
                                                   viewportHalfWidth, viewportHalfHeight,
                                                   viewport.x, viewport.y);
            Vector3 p2 = CameraController::project(v2, viewProjMatrix,
                                                   viewportHalfWidth, viewportHalfHeight,
                                                   viewport.x, viewport.y);
            drawTriangle3D_Color_Preprojected(v0, v1, v2,
                                              p0, p1, p2,
                                              baseR, baseG, baseB,
                                              camera, viewport,
                                              framebuffer, zBuffer,
                                              lights, activeLightCount,
                                              useUniformColor,
                                              uniformColor);
        }
    };

}

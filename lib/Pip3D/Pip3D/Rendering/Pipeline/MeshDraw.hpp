#pragma once

#include "Core/Viewport.hpp"
#include "Camera/Camera.hpp"
#include "Camera/Frustum.hpp"
#include "Math/Algebra.hpp"
#include "Geometry/Mesh.hpp"
#include "Rendering/Display/ZBuffer.hpp"
#include "Rendering/Display/FrameBuffer.hpp"
#include "Rendering/Lighting/Lighting.hpp"
#include "Rasterizer.hpp"
#include "Shading.hpp"
#include "Camera/Camera.hpp"

namespace pip3D
{

    class MeshRenderer
    {
    public:
        static constexpr float RGB565_RED_TO_FLOAT = 0.03225806451612903226f;
        static constexpr float RGB565_GREEN_TO_FLOAT = 0.01587301587301587302f;
        static constexpr float RGB565_BLUE_TO_FLOAT = 0.03225806451612903226f;

        static void decodeColorToFloat(uint16_t color,
                                       float &baseR,
                                       float &baseG,
                                       float &baseB)
        {
            baseR = ((color >> 11) & 0x1F) * RGB565_RED_TO_FLOAT;
            baseG = ((color >> 5) & 0x3F) * RGB565_GREEN_TO_FLOAT;
            baseB = (color & 0x1F) * RGB565_BLUE_TO_FLOAT;
        }

    private:
        static void drawTriangle3D_Color_Preprojected(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
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
            (void)viewProjMatrix;
            (void)backfaceCullingEnabled;
            (void)statsTrianglesTotal;
            (void)statsTrianglesBackfaceCulled;

            const int16_t bandTop = currentBandOffsetY();
            const int16_t bandBottom = static_cast<int16_t>(bandTop + currentBandHeight());
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

                Vector3 fragPos = v0;
                Vector3 viewDir = camera.position - v0;

                if (camera.projectionType == PERSPECTIVE)
                {
                    if (p0.z <= 0.0f && p1.z <= 0.0f && p2.z <= 0.0f)
                        return;
                }
                normal.normalize();
                viewDir.normalize();

                float finalR, finalG, finalB;
                Shading::calculateLighting(fragPos, normal, viewDir,
                                           lights, activeLightCount,
                                           baseR, baseG, baseB,
                                           finalR, finalG, finalB);

                if (Rasterizer::g_fogState.enabled)
                {
                    float dist = (camera.position - fragPos).length();
                    float fogFactor = (dist - Rasterizer::g_fogState.worldNear) * Rasterizer::g_fogState.worldScale;
                    if (fogFactor < 0.0f) fogFactor = 0.0f;
                    if (fogFactor > 1.0f) fogFactor = 1.0f;

                    finalR = finalR * (1.0f - fogFactor) + Rasterizer::g_fogState.color_r * fogFactor;
                    finalG = finalG * (1.0f - fogFactor) + Rasterizer::g_fogState.color_g_f * fogFactor;
                    finalB = finalB * (1.0f - fogFactor) + Rasterizer::g_fogState.color_b_f * fogFactor;
                }

                shadedColor = Shading::quantizeColor(finalR, finalG, finalB);
            }
            else
            {
                if (camera.projectionType == PERSPECTIVE)
                {
                    if (p0.z <= 0.0f && p1.z <= 0.0f && p2.z <= 0.0f)
                        return;
                }
            }

            Rasterizer::fillTriangle(lp0.x, lp0.y, lp0.z,
                                     lp1.x, lp1.y, lp1.z,
                                     lp2.x, lp2.y, lp2.z,
                                     shadedColor,
                                     frameBuffer,
                                     zBuffer,
                                     framebufferConfig);
        }

        static void drawTriangle3D_Color(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
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
            Vector3 p0 = CameraController::project(v0, viewProjMatrix, viewport);
            Vector3 p1 = CameraController::project(v1, viewProjMatrix, viewport);
            Vector3 p2 = CameraController::project(v2, viewProjMatrix, viewport);

            drawTriangle3D_Color_Preprojected(v0, v1, v2,
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
            if (camera.projectionType == PERSPECTIVE)
            {
                const Vector3 camPos = camera.position;
                const Vector3 camFwd = camera.forward();
                const float nearD = camera.nearPlane;

                float d0 = (v0 - camPos).dot(camFwd);
                float d1 = (v1 - camPos).dot(camFwd);
                float d2 = (v2 - camPos).dot(camFwd);

                if (d0 < nearD && d1 < nearD && d2 < nearD)
                {
                    return;
                }

                if (d0 >= nearD && d1 >= nearD && d2 >= nearD)
                {
                    drawTriangle3D_Color_Preprojected(v0, v1, v2,
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
                    return;
                }

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
                return;
            }

            drawTriangle3D_Color_Preprojected(v0, v1, v2,
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
            if (camera.projectionType == PERSPECTIVE)
            {
                const Vector3 camPos = camera.position;
                const Vector3 camFwd = camera.forward();
                const float nearD = camera.nearPlane;

                Vector3 inVerts[3] = {v0, v1, v2};
                float dist[3];
                for (int i = 0; i < 3; ++i)
                {
                    dist[i] = (inVerts[i] - camPos).dot(camFwd);
                }

                auto isInside = [&](int i) -> bool
                {
                    return dist[i] >= nearD;
                };

                auto intersect = [&](const Vector3 &a, const Vector3 &b, float da, float db) -> Vector3
                {
                    float denom = (db - da);
                    if (fabsf(denom) < 1e-6f)
                        return a;
                    float t = (nearD - da) / denom;
                    if (t < 0.0f)
                        t = 0.0f;
                    if (t > 1.0f)
                        t = 1.0f;
                    return a + (b - a) * t;
                };

                Vector3 clipped[4];
                int outCount = 0;

                for (int i = 0; i < 3; ++i)
                {
                    int j = (i + 1) % 3;
                    bool in0 = isInside(i);
                    bool in1 = isInside(j);
                    const Vector3 &P0 = inVerts[i];
                    const Vector3 &P1 = inVerts[j];
                    float d0 = dist[i];
                    float d1 = dist[j];

                    if (in0 && in1)
                    {
                        clipped[outCount++] = P1;
                    }
                    else if (in0 && !in1)
                    {
                        clipped[outCount++] = intersect(P0, P1, d0, d1);
                    }
                    else if (!in0 && in1)
                    {
                        clipped[outCount++] = intersect(P0, P1, d0, d1);
                        clipped[outCount++] = P1;
                    }
                }

                if (outCount < 3)
                {
                    return;
                }

                if (outCount == 3)
                {
                    drawTriangle3D_Color(clipped[0], clipped[1], clipped[2],
                                         baseR, baseG, baseB,
                                         camera, viewport, viewProjMatrix,
                                         framebuffer, zBuffer,
                                         lights, activeLightCount,
                                         backfaceCullingEnabled,
                                         statsTrianglesTotal,
                                         statsTrianglesBackfaceCulled,
                                         useUniformColor,
                                         uniformColor);
                    return;
                }

                if (outCount == 4)
                {
                    drawTriangle3D_Color(clipped[0], clipped[1], clipped[2],
                                         baseR, baseG, baseB,
                                         camera, viewport, viewProjMatrix,
                                         framebuffer, zBuffer,
                                         lights, activeLightCount,
                                         backfaceCullingEnabled,
                                         statsTrianglesTotal,
                                         statsTrianglesBackfaceCulled,
                                         useUniformColor,
                                         uniformColor);

                    drawTriangle3D_Color(clipped[0], clipped[2], clipped[3],
                                         baseR, baseG, baseB,
                                         camera, viewport, viewProjMatrix,
                                         framebuffer, zBuffer,
                                         lights, activeLightCount,
                                         backfaceCullingEnabled,
                                         statsTrianglesTotal,
                                         statsTrianglesBackfaceCulled,
                                         useUniformColor,
                                         uniformColor);
                    return;
                }
            }

            drawTriangle3D_Color(v0, v1, v2,
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

    public:
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

        static void drawMesh(Mesh *mesh,
                             const Camera &camera,
                             const Viewport &viewport,
                             const Frustum &frustum,
                             const Matrix4x4 &viewProjMatrix,
                             FrameBuffer &framebuffer,
                             ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                             const Light *lights,
                             int activeLightCount,
                             bool backfaceCullingEnabled,
                             uint32_t &statsTrianglesTotal,
                             uint32_t &statsTrianglesBackfaceCulled,
                             ShadingMode shadingMode = SHADING_FLAT)
        {
            if (!mesh || !mesh->isVisible())
                return;

            mesh->updateTransform();

            Vector3 center = mesh->center();
            float radius = mesh->radius();
            if (!frustum.sphere(center, radius))
                return;

            const uint16_t meshColor565 = mesh->color().rgb565;
            float baseR, baseG, baseB;
            decodeColorToFloat(meshColor565, baseR, baseG, baseB);

            const uint16_t faceCount = mesh->numFaces();
            if (faceCount == 0)
                return;

            const Vector3 &camFwd = camera.forward();
            const Vector3 &camPos = camera.position;
            const float nearPlane = camera.nearPlane;
            const Vector3 cameraBackward = camFwd * -1.0f;

            bool useUniformColor = mesh->getSingleColorLighting();
                uint16_t uniformColor = 0;
                if (useUniformColor)
                {
                    Vector3 meshNormal = mesh->normal(0);
                    Vector3 viewDir = camPos - center; 
                    viewDir.normalize();

                    float finalR, finalG, finalB;
                    Shading::calculateLighting(center, meshNormal, viewDir,
                                               lights, activeLightCount,
                                               baseR, baseG, baseB,
                                               finalR, finalG, finalB,
                                               true);

                    if (Rasterizer::g_fogState.enabled)
                    {
                        float dist = (camPos - center).length();
                        float fogFactor = (dist - Rasterizer::g_fogState.worldNear) * Rasterizer::g_fogState.worldScale;
                        if (fogFactor < 0.0f) fogFactor = 0.0f;
                        if (fogFactor > 1.0f) fogFactor = 1.0f;

                        finalR = finalR * (1.0f - fogFactor) + Rasterizer::g_fogState.color_r * fogFactor;
                        finalG = finalG * (1.0f - fogFactor) + Rasterizer::g_fogState.color_g_f * fogFactor;
                        finalB = finalB * (1.0f - fogFactor) + Rasterizer::g_fogState.color_b_f * fogFactor;
                    }

                    uniformColor = Shading::quantizeColor(finalR, finalG, finalB);
                }

            const uint16_t vertexCountUsed = mesh->numVertices();
            if (!mesh->ensureProjectionCache(vertexCountUsed))
                return;

            const Vector3 *localVerts = nullptr;
            if (mesh->ensureDecodedVertexCache())
                localVerts = mesh->getCachedLocalVertices();

            Vector3 *worldVerts = mesh->getCachedWorldVertices();
            Vector3 *screenVerts = mesh->getCachedScreenVertices();
            const uint32_t frameStamp = currentFrameStamp();
            const Matrix4x4 &meshTransform = mesh->getTransform();
            const int16_t bandTop = currentBandOffsetY();
            const int16_t bandBottom = static_cast<int16_t>(bandTop + currentBandHeight());
            const float viewportWidth = static_cast<float>(viewport.width);
            const float viewportHalfWidth = viewportWidth * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
            const bool usePerspectiveFacing = camera.projectionType == PERSPECTIVE || camera.projectionType == FISHEYE;

            if (mesh->getCachedProjectionFrameStamp() != frameStamp)
            {
                for (uint16_t i = 0; i < vertexCountUsed; ++i)
                {
                    Vector3 localPos = localVerts ? localVerts[i] : mesh->decodePosition(mesh->vert(i));
                    Vector3 worldPos = meshTransform.transformNoDiv(localPos);
                    worldVerts[i] = worldPos;
                    screenVerts[i] = CameraController::project(worldPos, viewProjMatrix,
                                                               viewportHalfWidth, viewportHalfHeight,
                                                               viewport.x, viewport.y);
                }

                mesh->setCachedProjectionFrameStamp(frameStamp);
            }

            thread_local static std::vector<Vector3> vertexColors;
                if (shadingMode == SHADING_GOURAUD && !useUniformColor)
                {
                    if (vertexColors.size() < vertexCountUsed)
                        vertexColors.resize(vertexCountUsed);

                    for (uint16_t vi = 0; vi < vertexCountUsed; ++vi)
                    {
                        Vector3 v = worldVerts[vi];
                        Vector3 n = mesh->normal(vi);
                        Vector3 viewDir = camPos - v; 
                        viewDir.normalize();

                        float r, g, b;
                        Shading::calculateLighting(v, n, viewDir,
                                                   lights, activeLightCount,
                                                   baseR, baseG, baseB,
                                                   r, g, b);

                        if (Rasterizer::g_fogState.enabled)
                        {
                            float dist = (camPos - v).length();
                            float fogFactor = (dist - Rasterizer::g_fogState.worldNear) * Rasterizer::g_fogState.worldScale;
                            if (fogFactor < 0.0f) fogFactor = 0.0f;
                            if (fogFactor > 1.0f) fogFactor = 1.0f;

                            r = r * (1.0f - fogFactor) + Rasterizer::g_fogState.color_r * fogFactor;
                            g = g * (1.0f - fogFactor) + Rasterizer::g_fogState.color_g_f * fogFactor;
                            b = b * (1.0f - fogFactor) + Rasterizer::g_fogState.color_b_f * fogFactor;
                        }

                        vertexColors[vi] = Vector3(r, g, b);
                    }
                }

            const DisplayConfig &framebufferConfig = framebuffer.getConfig();

            for (uint16_t i = 0; i < faceCount; ++i)
            {
                statsTrianglesTotal++;

                const Face &face = mesh->face(i);
                uint16_t i0 = face.v0;
                uint16_t i1 = face.v1;
                uint16_t i2 = face.v2;

                const Vector3 &v0 = worldVerts[i0];
                const Vector3 &v1 = worldVerts[i1];
                const Vector3 &v2 = worldVerts[i2];

                if (backfaceCullingEnabled)
                {
                    Vector3 faceNormal = (v1 - v0).cross(v2 - v0);
                    float normalLenSq = faceNormal.lengthSquared();
                    if (normalLenSq <= 1e-10f)
                    {
                        statsTrianglesBackfaceCulled++;
                        continue;
                    }

                    float facing = 0.0f;
                    if (usePerspectiveFacing)
                        facing = faceNormal.dot(camPos - v0);
                    else
                        facing = faceNormal.dot(cameraBackward);

                    if (facing <= 0.0f)
                    {
                        statsTrianglesBackfaceCulled++;
                        continue;
                    }
                }

                const Vector3 &p0 = screenVerts[i0];
                const Vector3 &p1 = screenVerts[i1];
                const Vector3 &p2 = screenVerts[i2];

                float d0 = (v0 - camPos).dot(camFwd);
                float d1 = (v1 - camPos).dot(camFwd);
                float d2 = (v2 - camPos).dot(camFwd);

                if (d0 < nearPlane && d1 < nearPlane && d2 < nearPlane)
                    continue;

                bool partiallyClipped = (d0 < nearPlane || d1 < nearPlane || d2 < nearPlane);

                if (mesh->isTextured() && !partiallyClipped)
                {
                    const Vertex &vert0 = mesh->vert(i0);
                    const Vertex &vert1 = mesh->vert(i1);
                    const Vertex &vert2 = mesh->vert(i2);

                    Vector3 lp0 = p0;
                    Vector3 lp1 = p1;
                    Vector3 lp2 = p2;
                    lp0.y -= (float)bandTop;
                    lp1.y -= (float)bandTop;
                    lp2.y -= (float)bandTop;

                    Rasterizer::fillTriangleTextured(
                        lp0.x, lp0.y, lp0.z,
                        lp1.x, lp1.y, lp1.z,
                        lp2.x, lp2.y, lp2.z,
                        vert0.tu, vert0.tv,
                        vert1.tu, vert1.tv,
                        vert2.tu, vert2.tv,
                        d0, d1, d2,
                        *mesh->getTexture(),
                        framebuffer.getBuffer(),
                        zBuffer,
                        framebufferConfig);
                    continue;
                }

                if (!partiallyClipped)
                {
                    float minY = fminf(p0.y, fminf(p1.y, p2.y));
                    float maxY = fmaxf(p0.y, fmaxf(p1.y, p2.y));
                    if (maxY < bandTop || minY >= bandBottom)
                        continue;

                    float minX = fminf(p0.x, fminf(p1.x, p2.x));
                    float maxX = fmaxf(p0.x, fmaxf(p1.x, p2.x));
                    if (maxX < 0.0f || minX >= viewportWidth)
                        continue;
                }

                if (shadingMode == SHADING_GOURAUD && !useUniformColor && !partiallyClipped)
                {
                    const Vector3 &c0 = vertexColors[i0];
                    const Vector3 &c1 = vertexColors[i1];
                    const Vector3 &c2 = vertexColors[i2];

                    Vector3 lp0 = p0;
                    Vector3 lp1 = p1;
                    Vector3 lp2 = p2;
                    lp0.y -= (float)bandTop;
                    lp1.y -= (float)bandTop;
                    lp2.y -= (float)bandTop;

                    Rasterizer::fillTriangleSmooth(
                        (int16_t)lp0.x, (int16_t)lp0.y, lp0.z,
                        (int16_t)lp1.x, (int16_t)lp1.y, lp1.z,
                        (int16_t)lp2.x, (int16_t)lp2.y, lp2.z,
                        c0.x, c0.y, c0.z,
                        c1.x, c1.y, c1.z,
                        c2.x, c2.y, c2.z,
                        framebuffer.getBuffer(),
                        zBuffer,
                        framebufferConfig);
                    continue;
                }

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
        }

        static void drawWaterMesh(Mesh *mesh,
                                  const Camera &camera,
                                  const Viewport &viewport,
                                  const Frustum &frustum,
                                  const Matrix4x4 &viewProjMatrix,
                                  FrameBuffer &framebuffer,
                                  ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                  float time,
                                  const uint16_t *reflectionBuffer,
                                  uint16_t reflectionWidth,
                                  uint16_t reflectionHeight)
        {
            if (!mesh || !mesh->isVisible())
                return;

            mesh->updateTransform();

            Vector3 center = mesh->center();
            float radius = mesh->radius();
            if (!frustum.sphere(center, radius))
                return;

            const uint16_t faceCount = mesh->numFaces();
            if (faceCount == 0)
                return;

            const uint16_t vertexCountUsed = mesh->numVertices();
            if (!mesh->ensureProjectionCache(vertexCountUsed))
                return;

            const Vector3 *localVerts = nullptr;
            if (mesh->ensureDecodedVertexCache())
                localVerts = mesh->getCachedLocalVertices();

            Vector3 *worldVerts = mesh->getCachedWorldVertices();
            Vector3 *screenVerts = mesh->getCachedScreenVertices();
            const Matrix4x4 &meshTransform = mesh->getTransform();
            const int16_t bandTop = currentBandOffsetY();
            const int16_t bandBottom = static_cast<int16_t>(bandTop + currentBandHeight());
            const float viewportWidth = static_cast<float>(viewport.width);
            const float viewportHalfWidth = viewportWidth * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;

            Vector3 waterCenterProj = CameraController::project(center, viewProjMatrix, viewport);
            float waterYGlobal = waterCenterProj.y;

            for (uint16_t i = 0; i < vertexCountUsed; ++i)
            {
                Vector3 localPos = localVerts ? localVerts[i] : mesh->decodePosition(mesh->vert(i));
                Vector3 worldPos = meshTransform.transformNoDiv(localPos);
                worldVerts[i] = worldPos;
                screenVerts[i] = CameraController::project(worldPos, viewProjMatrix,
                                                           viewportHalfWidth, viewportHalfHeight,
                                                           viewport.x, viewport.y);
            }

            const DisplayConfig &framebufferConfig = framebuffer.getConfig();

            for (uint16_t i = 0; i < faceCount; ++i)
            {
                const Face &face = mesh->face(i);
                uint16_t i0 = face.v0;
                uint16_t i1 = face.v1;
                uint16_t i2 = face.v2;

                const Vector3 &p0 = screenVerts[i0];
                const Vector3 &p1 = screenVerts[i1];
                const Vector3 &p2 = screenVerts[i2];

                float minY = fminf(p0.y, fminf(p1.y, p2.y));
                float maxY = fmaxf(p0.y, fmaxf(p1.y, p2.y));
                if (maxY < bandTop || minY >= bandBottom)
                    continue;

                float minX = fminf(p0.x, fminf(p1.x, p2.x));
                float maxX = fmaxf(p0.x, fmaxf(p1.x, p2.x));
                if (maxX < 0.0f || minX >= viewportWidth)
                    continue;

                Vector3 lp0 = p0;
                Vector3 lp1 = p1;
                Vector3 lp2 = p2;
                lp0.y -= (float)bandTop;
                lp1.y -= (float)bandTop;
                lp2.y -= (float)bandTop;

                Rasterizer::fillTriangleWater(lp0.x, lp0.y, lp0.z,
                                              lp1.x, lp1.y, lp1.z,
                                              lp2.x, lp2.y, lp2.z,
                                              time,
                                              waterYGlobal,
                                              framebuffer.getSkybox(),
                                              framebuffer.getBuffer(),
                                              zBuffer,
                                              framebufferConfig,
                                              bandTop,
                                              bandBottom,
                                              reflectionBuffer,
                                              reflectionWidth,
                                              reflectionHeight);
            }
        }
    };

}
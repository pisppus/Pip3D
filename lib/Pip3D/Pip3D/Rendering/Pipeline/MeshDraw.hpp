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
            if (!frustum.testSphere(center, radius))
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

                const float cdx = camPos.x - center.x;
                const float cdy = camPos.y - center.y;
                const float cdz = camPos.z - center.z;
                const float clenSq = cdx * cdx + cdy * cdy + cdz * cdz;
                const float cinvLen = (clenSq > 1e-8f) ? FastMath::fastInvSqrt(clenSq) : 0.0f;
                const Vector3 viewDir(cdx * cinvLen, cdy * cinvLen, cdz * cinvLen);

                float finalR, finalG, finalB;
                Shading::calculateLighting(center, meshNormal, viewDir,
                                           lights, activeLightCount,
                                           baseR, baseG, baseB,
                                           finalR, finalG, finalB,
                                           true);

                if (Rasterizer::g_fogState.enabled)
                {
                    float dist = clenSq * cinvLen;
                    float fogFactor = (dist - Rasterizer::g_fogState.worldNear) * Rasterizer::g_fogState.worldScale;
                    if (fogFactor < 0.0f)
                        fogFactor = 0.0f;
                    if (fogFactor > 1.0f)
                        fogFactor = 1.0f;

                    finalR = finalR * (1.0f - fogFactor) + Rasterizer::g_fogState.color_r * fogFactor;
                    finalG = finalG * (1.0f - fogFactor) + Rasterizer::g_fogState.color_g_f * fogFactor;
                    finalB = finalB * (1.0f - fogFactor) + Rasterizer::g_fogState.color_b_f * fogFactor;
                }

                uniformColor = Color::fromFloat(finalR, finalG, finalB).rgb565;
            }

            const uint16_t vertexCountUsed = mesh->numVertices();
            if (!mesh->ensureProjectionCache(vertexCountUsed))
                return;

            const Vector3 *localVerts = nullptr;
            if (mesh->ensureDecodedVertexCache())
                localVerts = mesh->getCachedLocalVertices();

            Vector3 *worldVerts = mesh->getCachedWorldVertices();
            Vector3 *screenVerts = mesh->getCachedScreenVertices();
            const uint32_t frameStamp = g_frameStamp;
            const Matrix4x4 &meshTransform = mesh->getTransform();
            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
            const float viewportWidth = static_cast<float>(viewport.width);
            const float viewportHalfWidth = viewportWidth * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
            const bool usePerspectiveFacing = camera.projectionType == PERSPECTIVE;

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
                    const Vector3 &v = worldVerts[vi];
                    Vector3 n = mesh->normal(vi);

                    const float dx = camPos.x - v.x;
                    const float dy = camPos.y - v.y;
                    const float dz = camPos.z - v.z;
                    const float lenSq = dx * dx + dy * dy + dz * dz;
                    const float invLen = (lenSq > 1e-8f) ? FastMath::fastInvSqrt(lenSq) : 0.0f;
                    const Vector3 viewDir(dx * invLen, dy * invLen, dz * invLen);

                    float r, g, b;
                    Shading::calculateLighting(v, n, viewDir,
                                               lights, activeLightCount,
                                               baseR, baseG, baseB,
                                               r, g, b);

                    if (Rasterizer::g_fogState.enabled)
                    {
                        float dist = lenSq * invLen;
                        float fogFactor = (dist - Rasterizer::g_fogState.worldNear) * Rasterizer::g_fogState.worldScale;
                        if (fogFactor < 0.0f)
                            fogFactor = 0.0f;
                        if (fogFactor > 1.0f)
                            fogFactor = 1.0f;

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

                const Vector3 &p0 = screenVerts[i0];
                const Vector3 &p1 = screenVerts[i1];
                const Vector3 &p2 = screenVerts[i2];

                const float v0x = v0.x, v0y = v0.y, v0z = v0.z;
                const float d0 = (v0x - camPos.x) * camFwd.x +
                                 (v0y - camPos.y) * camFwd.y +
                                 (v0z - camPos.z) * camFwd.z;
                const float v1x = v1.x, v1y = v1.y, v1z = v1.z;
                const float d1 = (v1x - camPos.x) * camFwd.x +
                                 (v1y - camPos.y) * camFwd.y +
                                 (v1z - camPos.z) * camFwd.z;
                const float v2x = v2.x, v2y = v2.y, v2z = v2.z;
                const float d2 = (v2x - camPos.x) * camFwd.x +
                                 (v2y - camPos.y) * camFwd.y +
                                 (v2z - camPos.z) * camFwd.z;

                if (d0 < nearPlane && d1 < nearPlane && d2 < nearPlane)
                    continue;

                bool partiallyClipped = (d0 < nearPlane || d1 < nearPlane || d2 < nearPlane);

                if (backfaceCullingEnabled)
                {
                    const float area = (p1.x - p0.x) * (p2.y - p0.y) -
                                       (p2.x - p0.x) * (p1.y - p0.y);

                    if (fabsf(area) > 1.0f && area >= 0.0f)
                    {
                        statsTrianglesBackfaceCulled++;
                        continue;
                    }
                }

                if (mesh->isTextured())
                {
                    const Vertex &vert0 = mesh->vert(i0);
                    const Vertex &vert1 = mesh->vert(i1);
                    const Vertex &vert2 = mesh->vert(i2);

                    float lr0, lg0, lb0, lr1, lg1, lb1, lr2, lg2, lb2;
                    if (shadingMode == SHADING_GOURAUD && !useUniformColor)
                    {
                        Vector3 n0 = mesh->normal(i0);
                        Vector3 vd0 = camPos - v0;
                        vd0.normalize();
                        Shading::calculateLighting(v0, n0, vd0, lights, activeLightCount, baseR, baseG, baseB, lr0, lg0, lb0);

                        Vector3 n1 = mesh->normal(i1);
                        Vector3 vd1 = camPos - v1;
                        vd1.normalize();
                        Shading::calculateLighting(v1, n1, vd1, lights, activeLightCount, baseR, baseG, baseB, lr1, lg1, lb1);

                        Vector3 n2 = mesh->normal(i2);
                        Vector3 vd2 = camPos - v2;
                        vd2.normalize();
                        Shading::calculateLighting(v2, n2, vd2, lights, activeLightCount, baseR, baseG, baseB, lr2, lg2, lb2);
                    }
                    else
                    {
                        Vector3 faceNormal = (v1 - v0).cross(v2 - v0);
                        faceNormal.normalize();
                        const Vector3 centroid = (v0 + v1 + v2) * (1.0f / 3.0f);
                        Vector3 viewDir = camPos - centroid;
                        viewDir.normalize();
                        Shading::calculateLighting(centroid, faceNormal, viewDir, lights, activeLightCount, baseR, baseG, baseB, lr0, lg0, lb0);
                        lr1 = lr0;
                        lg1 = lg0;
                        lb1 = lb0;
                        lr2 = lr0;
                        lg2 = lg0;
                        lb2 = lb0;
                    }

                    if (!partiallyClipped)
                    {
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
                            lr0, lg0, lb0,
                            lr1, lg1, lb1,
                            lr2, lg2, lb2,
                            *mesh->getTexture(),
                            framebuffer.getBuffer(),
                            zBuffer,
                            framebufferConfig);
                    }
                    else
                    {
                        ClipVert cv[3] = {
                            {v0, vert0.tu, vert0.tv, d0, lr0, lg0, lb0},
                            {v1, vert1.tu, vert1.tv, d1, lr1, lg1, lb1},
                            {v2, vert2.tu, vert2.tv, d2, lr2, lg2, lb2}};
                        clipAndDrawNearTextured(cv, nearPlane,
                                                camera, viewport, viewProjMatrix,
                                                framebuffer, zBuffer,
                                                *mesh->getTexture());
                    }
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

                if (!partiallyClipped)
                {
                    drawTriangle3D_Color_Preprojected(v0, v1, v2,
                                                      p0, p1, p2,
                                                      baseR, baseG, baseB,
                                                      camera, viewport,
                                                      framebuffer, zBuffer,
                                                      lights, activeLightCount,
                                                      useUniformColor,
                                                      uniformColor);
                }
                else
                {
                    const Vector3 inVerts[3] = {v0, v1, v2};
                    const float dist[3] = {d0, d1, d2};
                    clipAndDrawNear(inVerts, dist, nearPlane,
                                    baseR, baseG, baseB,
                                    camera, viewport, viewProjMatrix,
                                    framebuffer, zBuffer,
                                    lights, activeLightCount,
                                    useUniformColor,
                                    uniformColor);
                }
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
            if (!frustum.testSphere(center, radius))
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
            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
            const float viewportWidth = static_cast<float>(viewport.width);
            const float viewportHalfWidth = viewportWidth * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;

            for (uint16_t i = 0; i < vertexCountUsed; ++i)
            {
                Vector3 localPos = localVerts ? localVerts[i] : mesh->decodePosition(mesh->vert(i));
                Vector3 worldPos = meshTransform.transformNoDiv(localPos);
                worldVerts[i] = worldPos;
                screenVerts[i] = CameraController::project(worldPos, viewProjMatrix,
                                                           viewportHalfWidth, viewportHalfHeight,
                                                           viewport.x, viewport.y);
            }

            float waterYGlobal = screenVerts[0].y;
            for (uint16_t i = 1; i < vertexCountUsed; ++i)
            {
                if (screenVerts[i].y < waterYGlobal)
                    waterYGlobal = screenVerts[i].y;
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
                                              reflectionBuffer,
                                              reflectionWidth,
                                              reflectionHeight);
            }
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

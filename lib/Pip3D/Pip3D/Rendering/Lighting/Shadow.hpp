#pragma once

#include <stdint.h>
#include <math.h>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Lighting.hpp"
#include "Rendering/Display/FrameBuffer.hpp"
#include "Rendering/Display/ZBuffer.hpp"
#include "Rendering/Pipeline/Rasterizer.hpp"
#include "Camera/Camera.hpp"
#include "Geometry/Mesh.hpp"
#include "Geometry/Instance.hpp"

namespace pip3D
{
    class ShadowProjector
    {
    public:
        struct ShadowPlane
        {
            Vector3 normal;
            float d;

            ShadowPlane() : normal(0, 1, 0), d(0) {}
            ShadowPlane(const Vector3 &n, float distance) : normal(n), d(distance)
            {
                normal.normalize();
            }

            static ShadowPlane fromPointAndNormal(const Vector3 &point, const Vector3 &normal)
            {
                Vector3 n = normal;
                n.normalize();
                float d = -(n.x * point.x + n.y * point.y + n.z * point.z);
                return ShadowPlane(n, d);
            }
        };
    };

    struct ShadowSettings
    {
        bool enabled;
        Color shadowColor;
        float shadowOpacity;
        float shadowOffset;
        bool softEdges;
        ShadowProjector::ShadowPlane plane;

        ShadowSettings()
            : enabled(true), shadowColor(Color::fromRGB888(20, 20, 30)),
              shadowOpacity(0.7f),
              shadowOffset(0.0025f), softEdges(true), plane(Vector3(0, 1, 0), 0)
        {
        }
    };

    class ShadowRenderer
    {
    private:
        __attribute__((always_inline)) static inline bool isFiniteProjectedPoint(const Vector3 &p)
        {
            return isfinite(p.x) && isfinite(p.y) && isfinite(p.z);
        }

        __attribute__((always_inline)) static inline bool isShadowProjectionReasonable(const Vector3 &p0,
                                                                                       const Vector3 &p1,
                                                                                       const Vector3 &p2,
                                                                                       const Viewport &viewport)
        {
            if (!isFiniteProjectedPoint(p0) || !isFiniteProjectedPoint(p1) || !isFiniteProjectedPoint(p2))
                return false;

            float minX = fminf(p0.x, fminf(p1.x, p2.x));
            float maxX = fmaxf(p0.x, fmaxf(p1.x, p2.x));
            float minY = fminf(p0.y, fminf(p1.y, p2.y));
            float maxY = fmaxf(p0.y, fmaxf(p1.y, p2.y));

            const float maxReasonableWidth = static_cast<float>(viewport.width) * 6.0f;
            const float maxReasonableHeight = static_cast<float>(viewport.height) * 6.0f;

            if ((maxX - minX) > maxReasonableWidth || (maxY - minY) > maxReasonableHeight)
                return false;

            if (maxX < -static_cast<float>(viewport.width) * 3.0f || minX > static_cast<float>(viewport.width) * 4.0f)
                return false;
            if (maxY < -static_cast<float>(viewport.height) * 3.0f || minY > static_cast<float>(viewport.height) * 4.0f)
                return false;

            return true;
        }

        __attribute__((always_inline)) static inline void computeShadowColorAndAlpha(
            const ShadowSettings &shadowSettings,
            uint16_t &shadowColorOut,
            uint8_t &baseAlphaOut)
        {
            float opacity = clamp(shadowSettings.shadowOpacity, 0.0f, 1.0f);
            uint16_t srcColor = shadowSettings.shadowColor.rgb565;

            uint16_t r = (uint16_t)(((srcColor >> 11) & 0x1F) * opacity);
            uint16_t g = (uint16_t)(((srcColor >> 5) & 0x3F) * opacity);
            uint16_t b = (uint16_t)((srcColor & 0x1F) * opacity);
            shadowColorOut = (uint16_t)((r << 11) | (g << 5) | b);
            baseAlphaOut = (uint8_t)(opacity * COLOR_BYTE_MAX_F);
        }

        static void renderShadowTriangleInternal(
            const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
            const Matrix4x4 &viewProjMatrix,
            const Viewport &viewport,
            float viewportHalfWidth, float viewportHalfHeight,
            int16_t bandTop, int16_t bandBottom,
            uint16_t shadowColor, uint8_t baseAlpha,
            uint16_t *frameBuffer,
            ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
            const DisplayConfig &framebufferConfig,
            float depthBias)
        {
            Vector3 p0 = CameraController::project(v0, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
            Vector3 p1 = CameraController::project(v1, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
            Vector3 p2 = CameraController::project(v2, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);

            if (!isShadowProjectionReasonable(p0, p1, p2, viewport))
                return;

            p0.z -= depthBias;
            p1.z -= depthBias;
            p2.z -= depthBias;

            float minY = fminf(p0.y, fminf(p1.y, p2.y));
            float maxY = fmaxf(p0.y, fmaxf(p1.y, p2.y));
            if (maxY < bandTop || minY >= bandBottom)
                return;

            Vector3 lp0 = p0;
            Vector3 lp1 = p1;
            Vector3 lp2 = p2;
            lp0.y -= (float)bandTop;
            lp1.y -= (float)bandTop;
            lp2.y -= (float)bandTop;

            Rasterizer::fillShadowTriangle((int16_t)lp0.x, (int16_t)lp0.y, lp0.z,
                                           (int16_t)lp1.x, (int16_t)lp1.y, lp1.z,
                                           (int16_t)lp2.x, (int16_t)lp2.y, lp2.z,
                                           shadowColor,
                                           baseAlpha,
                                           frameBuffer,
                                           zBuffer,
                                           framebufferConfig,
                                           false);
        }

        static void clipAndRenderShadowTriangle(
            const Vector3 &sv0, const Vector3 &sv1, const Vector3 &sv2,
            const Camera &camera,
            const Viewport &viewport,
            const Matrix4x4 &viewProjMatrix,
            float viewportHalfWidth, float viewportHalfHeight,
            int16_t bandTop, int16_t bandBottom,
            uint16_t shadowColor, uint8_t baseAlpha,
            uint16_t *frameBuffer,
            ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
            const DisplayConfig &framebufferConfig,
            float depthBias)
        {
            if (camera.projectionType == PERSPECTIVE)
            {
                const Vector3 camPos = camera.position;
                const Vector3 camFwd = camera.forward();
                const float nearD = camera.nearPlane;

                Vector3 inVerts[3] = {sv0, sv1, sv2};
                float dist[3];
                for (int i = 0; i < 3; ++i)
                {
                    dist[i] = (inVerts[i] - camPos).dot(camFwd);
                }

                auto isInside = [&](int i) -> bool
                { return dist[i] >= nearD; };

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
                    return;

                if (outCount == 3)
                {
                    renderShadowTriangleInternal(clipped[0], clipped[1], clipped[2],
                                                 viewProjMatrix, viewport, viewportHalfWidth, viewportHalfHeight,
                                                 bandTop, bandBottom, shadowColor, baseAlpha,
                                                 frameBuffer, zBuffer, framebufferConfig, depthBias);
                }
                else if (outCount == 4)
                {
                    renderShadowTriangleInternal(clipped[0], clipped[1], clipped[2],
                                                 viewProjMatrix, viewport, viewportHalfWidth, viewportHalfHeight,
                                                 bandTop, bandBottom, shadowColor, baseAlpha,
                                                 frameBuffer, zBuffer, framebufferConfig, depthBias);
                    renderShadowTriangleInternal(clipped[0], clipped[2], clipped[3],
                                                 viewProjMatrix, viewport, viewportHalfWidth, viewportHalfHeight,
                                                 bandTop, bandBottom, shadowColor, baseAlpha,
                                                 frameBuffer, zBuffer, framebufferConfig, depthBias);
                }
            }
            else
            {
                renderShadowTriangleInternal(sv0, sv1, sv2,
                                             viewProjMatrix, viewport, viewportHalfWidth, viewportHalfHeight,
                                             bandTop, bandBottom, shadowColor, baseAlpha,
                                             frameBuffer, zBuffer, framebufferConfig, depthBias);
            }
        }

    public:
        static void drawMeshShadow(Mesh *mesh,
                                   bool shadowsEnabled,
                                   const ShadowSettings &shadowSettings,
                                   const Camera &camera,
                                   const Light *lights,
                                   int activeLightCount,
                                   const Matrix4x4 &viewProjMatrix,
                                   const Viewport &viewport,
                                   FrameBuffer &framebuffer,
                                   ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                   bool &backfaceCullingEnabled)
        {
            if (!mesh || !mesh->isVisible() || !shadowsEnabled || !shadowSettings.enabled)
                return;

            mesh->updateTransform();

            if (activeLightCount == 0)
                return;
            const Light &light = lights[0];
            if (light.type != LIGHT_DIRECTIONAL && light.type != LIGHT_POINT)
                return;

            const ShadowProjector::ShadowPlane &plane = shadowSettings.plane;
            Vector3 meshCenter = mesh->center();
            float meshRadius = mesh->radius();
            float centerDist = plane.normal.x * meshCenter.x + plane.normal.y * meshCenter.y + plane.normal.z * meshCenter.z + plane.d;
            if (centerDist + meshRadius <= 0.0f)
            {
                return;
            }

            Vector3 dirNorm(0.0f, -1.0f, 0.0f);
            if (light.type == LIGHT_DIRECTIONAL)
            {
                dirNorm = light.direction;
                dirNorm.normalize();
            }
            else if (light.type == LIGHT_POINT)
            {
                dirNorm = meshCenter - light.position;
                dirNorm.normalize();
            }

            const float planeY = -plane.d / plane.normal.y;
            const int16_t bandTop = currentBandOffsetY();
            const int16_t bandBottom = static_cast<int16_t>(bandTop + currentBandHeight());

            if (fabsf(dirNorm.y) > 0.01f)
            {
                float tProj = (planeY - meshCenter.y) / dirNorm.y;
                if (tProj > 0.0f)
                {
                    Vector3 shadowCenter = meshCenter + dirNorm * tProj;
                    float shadowRadius = meshRadius / fabsf(dirNorm.y);

                    Vector3 projCenter = CameraController::project(shadowCenter, viewProjMatrix, viewport);
                    Vector3 projEdgeX = CameraController::project(shadowCenter + camera.right() * shadowRadius, viewProjMatrix, viewport);
                    Vector3 projEdgeY = CameraController::project(shadowCenter + camera.upVec() * shadowRadius, viewProjMatrix, viewport);
                    float rScr = fmaxf(fabsf(projEdgeX.x - projCenter.x), fabsf(projEdgeY.y - projCenter.y));

                    if ((projCenter.y + rScr) < bandTop || (projCenter.y - rScr) >= bandBottom)
                    {
                        return;
                    }
                }
            }

            uint16_t shadowColor;
            uint8_t baseAlpha;
            computeShadowColorAndAlpha(shadowSettings, shadowColor, baseAlpha);

            float absLy = fabsf(dirNorm.y);
            float fadeFactor = 1.0f;
            if (absLy < 0.35f)
            {
                fadeFactor = (absLy - 0.12f) / (0.35f - 0.12f);
                if (fadeFactor < 0.0f)
                    fadeFactor = 0.0f;
            }
            baseAlpha = (uint8_t)(baseAlpha * fadeFactor);
            if (baseAlpha == 0)
            {
                return;
            }

            bool oldCulling = backfaceCullingEnabled;
            backfaceCullingEnabled = false;

            const float offsetY = shadowSettings.shadowOffset;
            const float depthBias = 0.0015f;
            const uint16_t vertexCount = mesh->numVertices();
            const uint16_t faceCount = mesh->numFaces();
            const float viewportHalfWidth = static_cast<float>(viewport.width) * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
            const Vector3 *localVerts = nullptr;
            if (mesh->ensureDecodedVertexCache())
                localVerts = mesh->getCachedLocalVertices();
            Vector3 *worldVerts = nullptr;
            const uint32_t frameStamp = currentFrameStamp();
            if (mesh->ensureProjectionCache(vertexCount))
            {
                worldVerts = mesh->getCachedWorldVertices();
                if (mesh->getCachedProjectionFrameStamp() != frameStamp)
                {
                    const Matrix4x4 &meshTransform = mesh->getTransform();
                    for (uint16_t vi = 0; vi < vertexCount; ++vi)
                    {
                        Vector3 localPos = localVerts ? localVerts[vi] : mesh->decodePosition(mesh->vert(vi));
                        worldVerts[vi] = meshTransform.transformNoDiv(localPos);
                    }
                }
            }

            const DisplayConfig &framebufferConfig = framebuffer.getConfig();
            uint16_t *const frameBuffer = framebuffer.getBuffer();

            thread_local static std::vector<Vector3> shadowVertsCache;
            if (shadowVertsCache.size() < vertexCount)
            {
                shadowVertsCache.resize(vertexCount);
            }

            const Vector3 &L_dir = dirNorm;
            const float ly = L_dir.y;
            const float signLy = (ly >= 0.0f) ? 1.0f : -1.0f;
            const float absLyVal = fabsf(ly);
            const float safeLy = (absLyVal < 0.22f) ? signLy * 0.22f : ly;

            for (uint16_t vi = 0; vi < vertexCount; ++vi)
            {
                Vector3 v = worldVerts ? worldVerts[vi] : mesh->vertex(vi);
                Vector3 sv;
                if (light.type == LIGHT_DIRECTIONAL)
                {
                    float t = (planeY - v.y) / safeLy;
                    sv = Vector3(v.x + t * L_dir.x, planeY, v.z + t * L_dir.z);
                }
                else
                {
                    Vector3 Ldir = v - light.position;
                    if (fabsf(Ldir.y) > 0.001f)
                    {
                        float t = (planeY - light.position.y) / Ldir.y;
                        sv = light.position + Ldir * t;
                    }
                    else
                    {
                        sv = Vector3(v.x, planeY, v.z);
                    }
                }
                sv.y += offsetY;
                shadowVertsCache[vi] = sv;
            }

            for (uint16_t i = 0; i < faceCount; ++i)
            {
                const Face &face = mesh->face(i);
                Vector3 v0;
                Vector3 v1;
                Vector3 v2;
                if (worldVerts)
                {
                    v0 = worldVerts[face.v0];
                    v1 = worldVerts[face.v1];
                    v2 = worldVerts[face.v2];
                }
                else
                {
                    v0 = mesh->vertex(face.v0);
                    v1 = mesh->vertex(face.v1);
                    v2 = mesh->vertex(face.v2);
                }

                float d0 = plane.normal.x * v0.x + plane.normal.y * v0.y + plane.normal.z * v0.z + plane.d;
                float d1 = plane.normal.x * v1.x + plane.normal.y * v1.y + plane.normal.z * v1.z + plane.d;
                float d2 = plane.normal.x * v2.x + plane.normal.y * v2.y + plane.normal.z * v2.z + plane.d;
                if (d0 <= 0.0f && d1 <= 0.0f && d2 <= 0.0f)
                    continue;

                Vector3 n = (v1 - v0).cross(v2 - v0);
                Vector3 L;
                if (light.type == LIGHT_DIRECTIONAL)
                {
                    L = Vector3(-dirNorm.x, -dirNorm.y, -dirNorm.z);
                }
                else
                {
                    L = light.position - v0;
                }
                float nl = n.dot(L);
                if (nl <= 0.0f)
                    continue;

                Vector3 sv0 = shadowVertsCache[face.v0];
                Vector3 sv1 = shadowVertsCache[face.v1];
                Vector3 sv2 = shadowVertsCache[face.v2];

                clipAndRenderShadowTriangle(sv0, sv1, sv2,
                                            camera, viewport, viewProjMatrix,
                                            viewportHalfWidth, viewportHalfHeight,
                                            bandTop, bandBottom, shadowColor, baseAlpha,
                                            frameBuffer, zBuffer, framebufferConfig, depthBias);
            }

            backfaceCullingEnabled = oldCulling;
        }

        static void drawMeshInstanceShadow(MeshInstance *instance,
                                           bool shadowsEnabled,
                                           const ShadowSettings &shadowSettings,
                                           const Camera &camera,
                                           const Light *lights,
                                           int activeLightCount,
                                           const Matrix4x4 &viewProjMatrix,
                                           const Viewport &viewport,
                                           FrameBuffer &framebuffer,
                                           ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                           bool &backfaceCullingEnabled)
        {
            if (!instance || !instance->isVisible() || !shadowsEnabled || !shadowSettings.enabled)
                return;

            Mesh *mesh = instance->getMesh();
            if (!mesh || !mesh->getCastShadows())
                return;

            if (activeLightCount == 0)
                return;
            const Light &light = lights[0];
            if (light.type != LIGHT_DIRECTIONAL && light.type != LIGHT_POINT)
                return;

            const Matrix4x4 &worldTransform = instance->transform();

            const ShadowProjector::ShadowPlane &plane = shadowSettings.plane;
            Vector3 instCenter = instance->center();
            float instRadius = instance->radius();
            float centerDist = plane.normal.x * instCenter.x + plane.normal.y * instCenter.y + plane.normal.z * instCenter.z + plane.d;
            if (centerDist + instRadius <= 0.0f)
            {
                return;
            }

            Vector3 dirNorm(0.0f, -1.0f, 0.0f);
            if (light.type == LIGHT_DIRECTIONAL)
            {
                dirNorm = light.direction;
                dirNorm.normalize();
            }
            else if (light.type == LIGHT_POINT)
            {
                dirNorm = instCenter - light.position;
                dirNorm.normalize();
            }

            const float planeY = -plane.d / plane.normal.y;
            const int16_t bandTop = currentBandOffsetY();
            const int16_t bandBottom = static_cast<int16_t>(bandTop + currentBandHeight());

            if (fabsf(dirNorm.y) > 0.01f)
            {
                float tProj = (planeY - instCenter.y) / dirNorm.y;
                if (tProj > 0.0f)
                {
                    Vector3 shadowCenter = instCenter + dirNorm * tProj;
                    float shadowRadius = instRadius / fabsf(dirNorm.y);

                    Vector3 projCenter = CameraController::project(shadowCenter, viewProjMatrix, viewport);
                    Vector3 projEdgeX = CameraController::project(shadowCenter + camera.right() * shadowRadius, viewProjMatrix, viewport);
                    Vector3 projEdgeY = CameraController::project(shadowCenter + camera.upVec() * shadowRadius, viewProjMatrix, viewport);
                    float rScr = fmaxf(fabsf(projEdgeX.x - projCenter.x), fabsf(projEdgeY.y - projCenter.y));

                    if ((projCenter.y + rScr) < bandTop || (projCenter.y - rScr) >= bandBottom)
                    {
                        return;
                    }
                }
            }

            uint16_t shadowColor;
            uint8_t baseAlpha;
            computeShadowColorAndAlpha(shadowSettings, shadowColor, baseAlpha);

            float absLy = fabsf(dirNorm.y);
            float fadeFactor = 1.0f;
            if (absLy < 0.35f)
            {
                fadeFactor = (absLy - 0.12f) / (0.35f - 0.12f);
                if (fadeFactor < 0.0f)
                    fadeFactor = 0.0f;
            }
            baseAlpha = (uint8_t)(baseAlpha * fadeFactor);
            if (baseAlpha == 0)
            {
                return;
            }

            bool oldCulling = backfaceCullingEnabled;
            backfaceCullingEnabled = false;

            const float offsetY = shadowSettings.shadowOffset;
            const float depthBias = 0.0015f;
            const uint16_t vertexCount = mesh->numVertices();
            const uint16_t faceCount = mesh->numFaces();
            const float viewportHalfWidth = static_cast<float>(viewport.width) * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
            const Vector3 *localVerts = nullptr;
            if (mesh->ensureDecodedVertexCache())
                localVerts = mesh->getCachedLocalVertices();
            Vector3 *worldVerts = nullptr;
            const uint32_t frameStamp = currentFrameStamp();
            if (instance->ensureProjectionCache(vertexCount))
            {
                worldVerts = instance->getCachedWorldVertices();
                if (instance->getCachedProjectionFrameStamp() != frameStamp)
                {
                    for (uint16_t vi = 0; vi < vertexCount; ++vi)
                    {
                        Vector3 local = localVerts ? localVerts[vi] : mesh->decodePosition(mesh->vert(vi));
                        worldVerts[vi] = worldTransform.transformNoDiv(local);
                    }
                }
            }

            const DisplayConfig &framebufferConfig = framebuffer.getConfig();
            uint16_t *const frameBuffer = framebuffer.getBuffer();

            thread_local static std::vector<Vector3> shadowVertsCache;
            if (shadowVertsCache.size() < vertexCount)
            {
                shadowVertsCache.resize(vertexCount);
            }

            const Vector3 &L_dir = dirNorm;
            const float ly = L_dir.y;
            const float signLy = (ly >= 0.0f) ? 1.0f : -1.0f;
            const float absLyVal = fabsf(ly);
            const float safeLy = (absLyVal < 0.22f) ? signLy * 0.22f : ly;

            for (uint16_t vi = 0; vi < vertexCount; ++vi)
            {
                Vector3 v = worldVerts ? worldVerts[vi] : worldTransform.transformNoDiv(localVerts ? localVerts[vi] : mesh->decodePosition(mesh->vert(vi)));
                Vector3 sv;
                if (light.type == LIGHT_DIRECTIONAL)
                {
                    float t = (planeY - v.y) / safeLy;
                    sv = Vector3(v.x + t * L_dir.x, planeY, v.z + t * L_dir.z);
                }
                else
                {
                    Vector3 Ldir = v - light.position;
                    if (fabsf(Ldir.y) > 0.001f)
                    {
                        float t = (planeY - light.position.y) / Ldir.y;
                        sv = light.position + Ldir * t;
                    }
                    else
                    {
                        sv = Vector3(v.x, planeY, v.z);
                    }
                }
                sv.y += offsetY;
                shadowVertsCache[vi] = sv;
            }

            for (uint16_t i = 0; i < faceCount; ++i)
            {
                const Face &face = mesh->face(i);
                Vector3 v0;
                Vector3 v1;
                Vector3 v2;
                if (worldVerts)
                {
                    v0 = worldVerts[face.v0];
                    v1 = worldVerts[face.v1];
                    v2 = worldVerts[face.v2];
                }
                else
                {
                    const Vertex &vert0 = mesh->vert(face.v0);
                    const Vertex &vert1 = mesh->vert(face.v1);
                    const Vertex &vert2 = mesh->vert(face.v2);

                    Vector3 local0 = localVerts ? localVerts[face.v0] : mesh->decodePosition(vert0);
                    Vector3 local1 = localVerts ? localVerts[face.v1] : mesh->decodePosition(vert1);
                    Vector3 local2 = localVerts ? localVerts[face.v2] : mesh->decodePosition(vert2);

                    v0 = worldTransform.transformNoDiv(local0);
                    v1 = worldTransform.transformNoDiv(local1);
                    v2 = worldTransform.transformNoDiv(local2);
                }

                float d0 = plane.normal.x * v0.x + plane.normal.y * v0.y + plane.normal.z * v0.z + plane.d;
                float d1 = plane.normal.x * v1.x + plane.normal.y * v1.y + plane.normal.z * v1.z + plane.d;
                float d2 = plane.normal.x * v2.x + plane.normal.y * v2.y + plane.normal.z * v2.z + plane.d;
                if (d0 <= 0.0f && d1 <= 0.0f && d2 <= 0.0f)
                    continue;

                Vector3 n = (v1 - v0).cross(v2 - v0);
                Vector3 L;
                if (light.type == LIGHT_DIRECTIONAL)
                {
                    L = Vector3(-dirNorm.x, -dirNorm.y, -dirNorm.z);
                }
                else
                {
                    L = light.position - v0;
                }
                float nl = n.dot(L);
                if (nl <= 0.0f)
                    continue;

                Vector3 sv0 = shadowVertsCache[face.v0];
                Vector3 sv1 = shadowVertsCache[face.v1];
                Vector3 sv2 = shadowVertsCache[face.v2];

                clipAndRenderShadowTriangle(sv0, sv1, sv2,
                                            camera, viewport, viewProjMatrix,
                                            viewportHalfWidth, viewportHalfHeight,
                                            bandTop, bandBottom, shadowColor, baseAlpha,
                                            frameBuffer, zBuffer, framebufferConfig, depthBias);
            }

            backfaceCullingEnabled = oldCulling;
        }
    };
}
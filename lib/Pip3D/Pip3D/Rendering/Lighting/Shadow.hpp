#pragma once

#include <math.h>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Lighting.hpp"
#include "Rendering/Buffers/FrameBuffer.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"
#include "Rendering/Pipeline/Rasterizer/Planar.hpp"
#include "Rendering/Pipeline/Rasterizer/Blob.hpp"
#include "Camera/Camera.hpp"
#include "Geometry/Mesh.hpp"
#include "Geometry/Instance.hpp"
#include "Rendering/Pipeline/DrawCache.hpp"

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
        bool shadowColorAuto;
        float shadowOpacity;
        float shadowOffset;
        bool softEdges;
        ShadowProjector::ShadowPlane plane;

        ShadowSettings()
            : enabled(true), shadowColor(Color::BLACK),
              shadowColorAuto(true),
              shadowOpacity(0.7f),
              shadowOffset(0.0025f), softEdges(true), plane(Vector3(0, 1, 0), 0)
        {
        }
    };

    class ShadowRenderer
    {
    public:
        static void drawMeshInstanceShadow(MeshInstance *instance,
                                           Mesh *shadowMesh,
                                           bool shadowsEnabled,
                                           const ShadowSettings &shadowSettings,
                                           const Camera &camera,
                                           const Light *lights,
                                           int activeLightCount,
                                           const Matrix4x4 &viewProjMatrix,
                                           const Viewport &viewport,
                                           FrameBuffer &framebuffer,
                                           ZBuffer *zBuffer,
                                           bool &backfaceCullingEnabled,
                                           DrawCache *drawCache,
                                           uint32_t shadowCacheGen)
        {
            if (!instance || !shadowMesh || !instance->isVisible() || !shadowsEnabled || !shadowSettings.enabled)
                return;

            Mesh *mesh = instance->getMesh();
            if (!mesh || !mesh->getCastShadows())
                return;
            if (activeLightCount == 0)
                return;
            const Light &light = lights[0];
            if (light.type != LIGHT_DIRECTIONAL && light.type != LIGHT_POINT)
                return;

            const ShadowProjector::ShadowPlane &plane = shadowSettings.plane;
            const Vector3 instCenter = instance->center();
            const float instRadius = instance->radius();
            const float centerDist = plane.normal.x * instCenter.x + plane.normal.y * instCenter.y + plane.normal.z * instCenter.z + plane.d;
            if (centerDist + instRadius <= 0.0f)
                return;

            uint16_t shadowColor;
            uint8_t baseAlpha;
            computeShadowColorAndAlpha(shadowSettings, shadowColor, baseAlpha);

            const int16_t bandTop = g_bandOffsetY;
            const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);

            renderShadowGeometry(instCenter, instRadius, instance->transform(), shadowMesh,
                                 light, shadowSettings, camera, viewProjMatrix, viewport,
                                 bandTop, bandBottom, shadowColor, baseAlpha,
                                 framebuffer.getBuffer(), zBuffer, framebuffer.getConfig(),
                                 backfaceCullingEnabled,
                                 drawCache, shadowCacheGen);
        }

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
            ZBuffer *zBuffer,
            const DisplayConfig &framebufferConfig,
            float depthBias)
        {
            Vector3 p0 = CameraController::project(v0, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
            Vector3 p1 = CameraController::project(v1, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
            Vector3 p2 = CameraController::project(v2, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);

            if (!isShadowProjectionReasonable(p0, p1, p2, viewport))
                return;

            p0.z += depthBias;
            p1.z += depthBias;
            p2.z += depthBias;

            const float minY = fminf(p0.y, fminf(p1.y, p2.y));
            const float maxY = fmaxf(p0.y, fmaxf(p1.y, p2.y));
            if (maxY < bandTop || minY >= bandBottom)
                return;

            const float bandTopF = static_cast<float>(bandTop);
            p0.y -= bandTopF;
            p1.y -= bandTopF;
            p2.y -= bandTopF;

            Rasterizer::fillPlanarShadowTriangle((int16_t)p0.x, (int16_t)p0.y, p0.z,
                                                 (int16_t)p1.x, (int16_t)p1.y, p1.z,
                                                 (int16_t)p2.x, (int16_t)p2.y, p2.z,
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
            ZBuffer *zBuffer,
            const DisplayConfig &framebufferConfig,
            float depthBias)
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

        __attribute__((always_inline)) static inline Vector3 computeDirNorm(const Light &light, const Vector3 &objectCenter)
        {
            if (light.type == LIGHT_POINT)
            {
                Vector3 dir = objectCenter - light.position;
                dir.normalize();
                return dir;
            }
            Vector3 dir = light.direction;
            dir.normalize();
            return dir;
        }

        __attribute__((always_inline)) static inline bool computeBandReject(const Vector3 &dirNorm,
                                                                            const Vector3 &center, float radius,
                                                                            float planeY,
                                                                            const Matrix4x4 &viewProjMatrix,
                                                                            const Viewport &viewport,
                                                                            const Camera &camera,
                                                                            int16_t bandTop, int16_t bandBottom)
        {
            if (fabsf(dirNorm.y) <= 0.01f)
                return false;

            const float tProj = (planeY - center.y) / dirNorm.y;
            if (tProj <= 0.0f)
                return false;

            const Vector3 shadowCenter = center + dirNorm * tProj;
            const float shadowRadius = radius / fabsf(dirNorm.y);

            const Vector3 projCenter = CameraController::project(shadowCenter, viewProjMatrix, viewport);
            const Vector3 projEdgeX = CameraController::project(shadowCenter + camera.right() * shadowRadius, viewProjMatrix, viewport);
            const Vector3 projEdgeY = CameraController::project(shadowCenter + camera.upVec() * shadowRadius, viewProjMatrix, viewport);
            const float rScr = fmaxf(fabsf(projEdgeX.x - projCenter.x), fabsf(projEdgeY.y - projCenter.y));

            return ((projCenter.y + rScr) < bandTop || (projCenter.y - rScr) >= bandBottom);
        }

        static void renderShadowGeometry(
            const Vector3 &objectCenter, float objectRadius,
            const Matrix4x4 &worldTransform,
            Mesh *shadowMesh,
            const Light &light,
            const ShadowSettings &shadowSettings,
            const Camera &camera,
            const Matrix4x4 &viewProjMatrix,
            const Viewport &viewport,
            int16_t bandTop, int16_t bandBottom,
            uint16_t shadowColor, uint8_t baseAlpha,
            uint16_t *frameBuffer,
            ZBuffer *zBuffer,
            const DisplayConfig &framebufferConfig,
            bool &backfaceCullingEnabled,
            DrawCache *drawCache,
            uint32_t shadowCacheGen)
        {
            const ShadowProjector::ShadowPlane &plane = shadowSettings.plane;

            const Vector3 dirNorm = computeDirNorm(light, objectCenter);
            const float planeY = -plane.d / plane.normal.y;

            if (computeBandReject(dirNorm, objectCenter, objectRadius, planeY,
                                  viewProjMatrix, viewport, camera, bandTop, bandBottom))
                return;

            const float absLy = fabsf(dirNorm.y);
            const float ly = dirNorm.y;
            const float signLy = (ly >= 0.0f) ? 1.0f : -1.0f;
            const float safeLy = (absLy < 0.22f) ? signLy * 0.22f : ly;
            const float invSafeLy = FastMath::fastReciprocal(safeLy);

            float fadeFactor = 1.0f;
            if (absLy < 0.35f)
            {
                fadeFactor = (absLy - 0.12f) * (1.0f / (0.35f - 0.12f));
                if (fadeFactor < 0.0f)
                    fadeFactor = 0.0f;
            }

            float heightDiff = objectCenter.y - planeY;
            if (heightDiff < 0.0f)
                heightDiff = 0.0f;
            const float maxHeightDist = objectRadius * 5.0f;
            if (maxHeightDist > 1e-4f)
            {
                float t = heightDiff * FastMath::fastReciprocal(maxHeightDist);
                if (t > 1.0f)
                    t = 1.0f;
                float sm = t * t * (3.0f - 2.0f * t);
                fadeFactor *= (1.0f - sm);
            }

            const float dx = objectCenter.x - camera.position.x;
            const float dy = objectCenter.y - camera.position.y;
            const float dz = objectCenter.z - camera.position.z;
            const float camDistSq = dx * dx + dy * dy + dz * dz;
            const float farFadeStart = camera.farPlane * 0.55f;
            const float farFadeEnd = camera.farPlane * 0.9f;
            if (camDistSq > farFadeStart * farFadeStart)
            {
                float distT = (camDistSq * FastMath::fastInvSqrt(camDistSq) - farFadeStart) *
                              FastMath::fastReciprocal(farFadeEnd - farFadeStart);
                if (distT > 1.0f)
                    distT = 1.0f;
                else if (distT < 0.0f)
                    distT = 0.0f;
                float distSm = distT * distT * (3.0f - 2.0f * distT);
                fadeFactor *= (1.0f - distSm);
            }

            baseAlpha = (uint8_t)(baseAlpha * fadeFactor);
            if (baseAlpha == 0)
                return;

            const bool oldCulling = backfaceCullingEnabled;
            backfaceCullingEnabled = false;

            const float offsetY = shadowSettings.shadowOffset;
            const float depthBias = 1.0f;

            const uint16_t vertexCount = shadowMesh->numVertices();
            const uint16_t faceCount = shadowMesh->numFaces();
            const float viewportHalfWidth = static_cast<float>(viewport.width) * 0.5f;
            const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;

            const Vertex *PIP3D_RESTRICT vbaseS = shadowMesh->vertexData();
            Vector3 *PIP3D_RESTRICT localVerts = static_cast<Vector3 *>(
                alloca(vertexCount * sizeof(Vector3)));
            for (uint16_t i = 0; i < vertexCount; ++i)
                localVerts[i] = shadowMesh->decodePosition(vbaseS[i]);

            Vector3 *worldVerts = (Vector3 *)alloca(vertexCount * sizeof(Vector3));

            Vector3 *shadowVertsCache = nullptr;
            bool needsCompute = false;
            bool fromDrawCache = false;
            if (drawCache && shadowCacheGen != 0)
            {
                shadowVertsCache = drawCache->acquireShadowVerts(shadowCacheGen, vertexCount, needsCompute);
                fromDrawCache = (shadowVertsCache != nullptr);
            }
            if (!fromDrawCache)
            {
                shadowVertsCache = (Vector3 *)alloca(vertexCount * sizeof(Vector3));
                needsCompute = true;
            }

            const bool recomputeShadow = needsCompute;
            const bool isDirectional = (light.type == LIGHT_DIRECTIONAL);
            const Vector3 negDirNorm(-dirNorm.x, -dirNorm.y, -dirNorm.z);
            const float dirX = dirNorm.x;
            const float dirZ = dirNorm.z;
            const float planeYMinusLightY = planeY - light.position.y;
            const float lightPosX = light.position.x;
            const float lightPosY = light.position.y;
            const float lightPosZ = light.position.z;

            for (uint16_t vi = 0; vi < vertexCount; ++vi)
            {
                const Vector3 v = worldTransform.transformNoDiv(localVerts[vi]);
                worldVerts[vi] = v;

                if (!recomputeShadow)
                    continue;

                Vector3 sv;
                if (isDirectional)
                {
                    const float t = (planeY - v.y) * invSafeLy;
                    sv = Vector3(v.x + t * dirX, planeY, v.z + t * dirZ);
                }
                else
                {
                    const float ldy = v.y - lightPosY;
                    if (fabsf(ldy) > 0.001f)
                    {
                        const float t = planeYMinusLightY / ldy;
                        sv = Vector3(lightPosX + (v.x - lightPosX) * t,
                                     planeY,
                                     lightPosZ + (v.z - lightPosZ) * t);
                    }
                    else
                    {
                        sv = Vector3(v.x, planeY, v.z);
                    }
                }
                sv.y += offsetY;
                shadowVertsCache[vi] = sv;
            }

            if (fromDrawCache && needsCompute)
                drawCache->commitShadowVerts(shadowCacheGen);

            const float pnX = plane.normal.x;
            const float pnY = plane.normal.y;
            const float pnZ = plane.normal.z;
            const float pd = plane.d;

            const Face *PIP3D_RESTRICT fbaseS = shadowMesh->faceData();
            for (uint16_t i = 0; i < faceCount; ++i)
            {
                const Face &face = fbaseS[i];
                const Vector3 v0 = worldVerts[face.v0];
                const Vector3 v1 = worldVerts[face.v1];
                const Vector3 v2 = worldVerts[face.v2];

                const float d0 = pnX * v0.x + pnY * v0.y + pnZ * v0.z + pd;
                const float d1 = pnX * v1.x + pnY * v1.y + pnZ * v1.z + pd;
                const float d2 = pnX * v2.x + pnY * v2.y + pnZ * v2.z + pd;
                if (d0 <= 0.0f && d1 <= 0.0f && d2 <= 0.0f)
                    continue;

                const Vector3 n = (v1 - v0).cross(v2 - v0);
                Vector3 L;
                if (isDirectional)
                {
                    L = negDirNorm;
                }
                else
                {
                    L = light.position - v0;
                }
                const float nl = n.dot(L);
                if (nl <= 0.0f)
                    continue;

                const Vector3 sv0 = shadowVertsCache[face.v0];
                const Vector3 sv1 = shadowVertsCache[face.v1];
                const Vector3 sv2 = shadowVertsCache[face.v2];

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
#include <vector>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Camera/Camera.hpp"
#include "Rendering/Buffers/ZBuffer.hpp"
#include "Rendering/Buffers/FrameBuffer.hpp"
#include "Rendering/Lighting/Lighting.hpp"
#include "Rendering/Pipeline/Culling.hpp"
#include "Rendering/Pipeline/DrawCache.hpp"
#include "Rendering/Pipeline/MeshDraw.hpp"
#include "Rendering/Pipeline/Shading.hpp"
#include "Rendering/Pipeline/Telemetry.hpp"
#include "Rendering/Pipeline/Rasterizer/Common.hpp"
#include "Rendering/Pipeline/Rasterizer/Smooth.hpp"
#include "Rendering/Pipeline/Rasterizer/Textured.hpp"
#include "Rendering/Renderer.hpp"

namespace pip3D
{

    PIP3D_HOT static int collectActiveLightsForBounds(const Vector3 &center, float radius,
                                                      const Light *allLights, int allLightCount,
                                                      Light *outLights, int maxLights)
    {
        int count = 0;

        for (int i = 0; i < allLightCount && count < maxLights; ++i)
        {
            if (allLights[i].type == LIGHT_DIRECTIONAL)
                outLights[count++] = allLights[i];
        }

        struct LightScore
        {
            int index;
            float score;
        };
        LightScore scores[32];
        int scoreCount = 0;

        for (int i = 0; i < allLightCount && scoreCount < 32; ++i)
        {
            const Light &l = allLights[i];
            if (l.type == LIGHT_POINT)
            {
                const float dx = l.position.x - center.x;
                const float dy = l.position.y - center.y;
                const float dz = l.position.z - center.z;
                const float distSq = dx * dx + dy * dy + dz * dz;

                const float maxDist = l.range + radius;
                if (l.range > 0.0f && distSq > maxDist * maxDist)
                    continue;

                float atten = 1.0f;
                if (l.range > 0.0f)
                    atten = FastMath::fastReciprocal(1.0f + distSq * l.invRangeSq);

                scores[scoreCount++] = {i, l.intensity * atten};
            }
        }

        if (scoreCount > maxLights - count)
        {

            for (int i = 1; i < scoreCount; ++i)
            {
                LightScore temp = scores[i];
                int j = i - 1;
                while (j >= 0 && scores[j].score < temp.score)
                {
                    scores[j + 1] = scores[j];
                    --j;
                }
                scores[j + 1] = temp;
            }
        }

        for (int i = 0; i < scoreCount && count < maxLights; ++i)
            outLights[count++] = allLights[scores[i].index];

        return count;
    }

    void Renderer::draw(MeshInstance *instance)
    {
        if (unlikely(!instance || !instance->isVisible()))
            return;

        Mesh *mesh = instance->getMesh();
        if (!mesh)
            return;

        if (shadowsEnabled)
        {
            if (instance->getBlobShadow())
                blobShadowQueue_.push_back(instance);
            else if (mesh->getCastShadows())
                shadowQueue_.push_back(instance);
        }

        opaqueQueue_.push_back(instance);
    }

    void Renderer::flushQueue()
    {

        const size_t n = opaqueQueue_.size();
        if (n > 1)
        {
            const Camera &cam = cameras[activeCameraIndex];
            const Vector3 camPos = cam.position;
            const Vector3 camFwd = cam.forward();

            float *PIP3D_RESTRICT eyeZ = static_cast<float *>(
                alloca(n * sizeof(float)));
            for (size_t i = 0; i < n; ++i)
                eyeZ[i] = Culling::computeEyeZ(opaqueQueue_[i]->center(), camPos, camFwd);

            for (size_t i = 1; i < n; ++i)
            {
                const float keyZ = eyeZ[i];
                MeshInstance *keyInst = opaqueQueue_[i];

                size_t j = i;
                while (j > 0 && eyeZ[j - 1] > keyZ)
                {
                    eyeZ[j] = eyeZ[j - 1];
                    opaqueQueue_[j] = opaqueQueue_[j - 1];
                    --j;
                }
                eyeZ[j] = keyZ;
                opaqueQueue_[j] = keyInst;
            }
        }

        for (size_t i = 0; i < opaqueQueue_.size(); ++i)
            drawMeshInstanceInternal(opaqueQueue_[i], false);

        for (size_t i = 0; i < shadowQueue_.size(); ++i)
            drawMeshInstanceShadow(shadowQueue_[i]);

        const float blobOpacity = shadowSettings.shadowOpacity;
        for (size_t i = 0; i < blobShadowQueue_.size(); ++i)
        {
            MeshInstance *inst = blobShadowQueue_[i];
            drawBlobShadow(inst->pos(), inst->radius(), blobOpacity);
        }
    }

    bool Renderer::clipAndDrawNearTextured(const DrawTelemetryClipVert inVerts[3],
                                           float nearD,
                                           const Camera &camera,
                                           const Viewport &viewport,
                                           const Matrix4x4 &viewProjMatrix,
                                           FrameBuffer &framebuffer,
                                           ZBuffer *zBuffer,
                                           const Texture &tex,
                                           const Mesh *meshForTelemetry,
                                           uint16_t faceIdxForTelemetry,
                                           uint32_t frameForTelemetry)
    {
        DrawTelemetryClipVert clipped[4];
        int outCount = 0;

#define PIP3D_CLIP_EDGE_T(IDX_A, IDX_B)                              \
    {                                                                \
        const DrawTelemetryClipVert &P0 = inVerts[IDX_A];            \
        const DrawTelemetryClipVert &P1 = inVerts[IDX_B];            \
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
            DrawTelemetryClipVert ip = lerpClipVert(P0, P1, t);      \
            if (in0)                                                 \
                clipped[outCount++] = ip;                            \
            else                                                     \
            {                                                        \
                clipped[outCount++] = ip;                            \
                clipped[outCount++] = P1;                            \
            }                                                        \
        }                                                            \
    }
        PIP3D_CLIP_EDGE_T(0, 1);
        PIP3D_CLIP_EDGE_T(1, 2);
        PIP3D_CLIP_EDGE_T(2, 0);
#undef PIP3D_CLIP_EDGE_T

        if (outCount < 3)
            return false;

        const float viewportHalfWidth = static_cast<float>(viewport.width) * 0.5f;
        const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
        const int16_t bandTop = g_bandOffsetY;
        const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
        const DisplayConfig &framebufferConfig = framebuffer.getConfig();
        const float viewportWidth = static_cast<float>(viewport.width);
        const float bandTopF = static_cast<float>(bandTop);
        uint16_t *const frameBuffer = framebuffer.getBuffer();

        Vector3 proj[4];
        for (int i = 0; i < outCount; ++i)
            proj[i] = CameraController::project(clipped[i].pos, viewProjMatrix,
                                                viewportHalfWidth, viewportHalfHeight, 0, 0);

        auto drawTri = [&](int a, int b, int c) -> bool
        {
            const Vector3 &p0 = proj[a];
            const Vector3 &p1 = proj[b];
            const Vector3 &p2 = proj[c];

            const float minY = (p0.y < p1.y) ? ((p0.y < p2.y) ? p0.y : p2.y)
                                             : ((p1.y < p2.y) ? p1.y : p2.y);
            const float maxY = (p0.y > p1.y) ? ((p0.y > p2.y) ? p0.y : p2.y)
                                             : ((p1.y > p2.y) ? p1.y : p2.y);
            if (maxY < bandTop || minY >= bandBottom)
            {
#if PIP3D_ENABLE_DRAW_TELEMETRY
                g_drawTelemetry.recordSkip(
                    SkipReason::BAND_Y, frameForTelemetry, faceIdxForTelemetry,
                    meshForTelemetry, clipped[a].d, clipped[b].d, clipped[c].d, 0.0f,
                    p0.x, p0.y, p0.z, p1.x, p1.y, p2.x, p2.y,
                    clipped[a].pos.x, clipped[a].pos.y, clipped[a].pos.z,
                    camera.position.x, camera.position.y, camera.position.z,
                    camera.forward().x, camera.forward().y, camera.forward().z,
                    nearD, bandTop, bandBottom, true, true);
#endif
                return false;
            }

            const float minX = (p0.x < p1.x) ? ((p0.x < p2.x) ? p0.x : p2.x)
                                             : ((p1.x < p2.x) ? p1.x : p2.x);
            const float maxX = (p0.x > p1.x) ? ((p0.x > p2.x) ? p0.x : p2.x)
                                             : ((p1.x > p2.x) ? p1.x : p2.x);
            if (maxX < 0.0f || minX >= viewportWidth)
            {
#if PIP3D_ENABLE_DRAW_TELEMETRY
                g_drawTelemetry.recordSkip(
                    SkipReason::FRUSTUM_X, frameForTelemetry, faceIdxForTelemetry,
                    meshForTelemetry, clipped[a].d, clipped[b].d, clipped[c].d, 0.0f,
                    p0.x, p0.y, p0.z, p1.x, p1.y, p2.x, p2.y,
                    clipped[a].pos.x, clipped[a].pos.y, clipped[a].pos.z,
                    camera.position.x, camera.position.y, camera.position.z,
                    camera.forward().x, camera.forward().y, camera.forward().z,
                    nearD, bandTop, bandBottom, true, true);
#endif
                return false;
            }

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
                tex, frameBuffer, zBuffer, framebufferConfig);
            return true;
        };

        bool drew = drawTri(0, 1, 2);
        if (outCount == 4)
            drew |= drawTri(0, 2, 3);
        return drew;
    }

    IRAM_ATTR void Renderer::drawMeshInstanceInternal(MeshInstance *instance, bool performFrustumCull)
    {
        if (!instance || !instance->isVisible())
            return;

        Mesh *mesh = instance->getMesh();
        if (!mesh)
            return;

        Vector3 center = instance->center();
        float radius = instance->radius();

        if (performFrustumCull)
        {
            if (!frustum.testSphere(center, radius))
            {
                statsInstancesFrustumCulled++;
                return;
            }
        }

        const Camera &cam = cameras[activeCameraIndex];
        const Vector3 &camFwd = cam.forward();

        const float zEye = Culling::computeEyeZ(center, cam.position, camFwd);

        float radiusPixels = 0.0f;
        if (zEye > cam.nearPlane)
        {
            const float projScale = Culling::ensureProjScale(cam, viewport);
            radiusPixels = Culling::computeScreenRadius(radius, zEye, projScale);

            if (radiusPixels < 1.0f)
            {
                statsInstancesTotal++;
                return;
            }
        }

        statsInstancesTotal++;

        const DisplayConfig &framebufferConfig = framebuffer.getConfig();

        if (occlusionCullingEnabled &&
            zEye - radius > cam.nearPlane &&
            Culling::isInstanceOccluded(
                center, radius, zEye, radiusPixels,
                viewport, viewProjMatrix, &zBuffer))
        {
            statsInstancesOcclusionCulled++;
            return;
        }

        const uint16_t instColor565 = instance->color().rgb565;
        float baseR, baseG, baseB;
        MeshRenderer::decodeColorToFloat(instColor565, baseR, baseG, baseB);

        const bool useUniformColor = mesh->getSingleColorLighting();
        uint16_t uniformColor = 0;

        Light localLights[4];
        const int localLightCount = collectActiveLightsForBounds(
            center, radius, lights.data(), activeLightCount, localLights, 4);

        const Matrix4x4 &worldTransform = instance->transform();
        const Vertex *PIP3D_RESTRICT vbase = mesh->vertexData();

        const auto &fog = Rasterizer::g_fogState;

        if (useUniformColor)
        {

            NormalMatrix nm(worldTransform);
            Vector3 localNormal = (mesh->numVertices() > 0) ? vbase[0].normal.get() : Vector3(0.0f, 1.0f, 0.0f);
            Vector3 worldNormal = nm.transform(localNormal);

            const float vx = cam.position.x - center.x;
            const float vy = cam.position.y - center.y;
            const float vz = cam.position.z - center.z;
            const float viewDistSq = vx * vx + vy * vy + vz * vz;
            const float invLen = (viewDistSq > 1e-8f) ? FastMath::fastInvSqrt(viewDistSq) : 0.0f;
            const Vector3 viewDir(vx * invLen, vy * invLen, vz * invLen);

            float litR, litG, litB;
            Shading::calculateLighting(center, worldNormal, viewDir,
                                       localLights, localLightCount,
                                       baseR, baseG, baseB,
                                       litR, litG, litB, true);

            const float dist = viewDistSq * invLen;
            Shading::applyFog(dist, litR, litG, litB, litR, litG, litB);

            uniformColor = Color::fromFloat(litR, litG, litB).rgb565;
        }

        const uint16_t vertexCountUsed = mesh->numVertices();
        const uint16_t faceCount = mesh->numFaces();

        DrawCache *const cache = &instance->drawCache();

        Vector3 *PIP3D_RESTRICT worldVerts = nullptr;
        Vector3 *PIP3D_RESTRICT screenVerts = nullptr;
        Vector3 *PIP3D_RESTRICT localVerts = nullptr;

        if (likely(cache->ensureCapacity(vertexCountUsed)))
        {
            worldVerts = cache->worldVerts();
            screenVerts = cache->screenVerts();
        }
        else
        {

            localVerts = static_cast<Vector3 *>(
                alloca(vertexCountUsed * sizeof(Vector3)));
            for (uint16_t i = 0; i < vertexCountUsed; ++i)
                localVerts[i] = mesh->decodePosition(vbase[i]);
        }

        const uint32_t frameStamp = g_frameStamp;
        const uint32_t instanceVersion = instance->version();

        const int16_t bandTop = g_bandOffsetY;
        const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
        const float bandTopF = static_cast<float>(bandTop);
        const float viewportWidth = static_cast<float>(viewport.width);
        const float viewportHalfWidth = viewportWidth * 0.5f;
        const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
        const Vector3 camPos = cam.position;
        const float nearPlane = cam.nearPlane;
        constexpr float kNearClipEps = 1e-4f;
        const float nearClip = nearPlane + kNearClipEps;
        const bool isTextured = mesh->isTextured();
        const bool doBackfaceCull = backfaceCullingEnabled;
        const bool gouraudShading = (shadingMode == SHADING_GOURAUD) && !useUniformColor;
        const uint32_t currentFrame = g_frameStamp;

        const Texture *const meshTexture = isTextured ? mesh->getTexture() : nullptr;

        NormalMatrix nmGouraud(worldTransform);

        if (likely(worldVerts))
        {
            const DrawCache::ProjState projState =
                cache->beginProjection(frameStamp, instanceVersion);

            if (projState == DrawCache::ProjState::NeedsTransformAndProject)
            {

                for (uint16_t i = 0; i < vertexCountUsed; ++i)
                {
                    const Vector3 local = mesh->decodePosition(vbase[i]);
                    const Vector3 world = worldTransform.transformNoDiv(local);
                    worldVerts[i] = world;
                    screenVerts[i] = CameraController::project(world, viewProjMatrix,
                                                               viewportHalfWidth, viewportHalfHeight,
                                                               0, 0);
                }
                cache->commitProjection(frameStamp, instanceVersion);
            }
            else if (projState == DrawCache::ProjState::NeedsReproject)
            {

                for (uint16_t i = 0; i < vertexCountUsed; ++i)
                {
                    screenVerts[i] = CameraController::project(worldVerts[i], viewProjMatrix,
                                                               viewportHalfWidth, viewportHalfHeight,
                                                               0, 0);
                }
                cache->commitProjection(frameStamp, instanceVersion);
            }
        }

        if (gouraudShading)
        {
            if (vertexColors_.size() < vertexCountUsed)
                vertexColors_.resize(vertexCountUsed);

            for (uint16_t vi = 0; vi < vertexCountUsed; ++vi)
            {
                Vector3 localNormal = vbase[vi].normal.get();
                Vector3 worldNormal = nmGouraud.transform(localNormal);

                const Vector3 v = worldVerts ? worldVerts[vi]
                                             : worldTransform.transformNoDiv(localVerts[vi]);

                const float vvx = camPos.x - v.x;
                const float vvy = camPos.y - v.y;
                const float vvz = camPos.z - v.z;
                const float vDistSq = vvx * vvx + vvy * vvy + vvz * vvz;
                const float vInvLen = (vDistSq > 1e-8f) ? FastMath::fastInvSqrt(vDistSq) : 0.0f;
                const Vector3 viewDir(vvx * vInvLen, vvy * vInvLen, vvz * vInvLen);

                float r, g, b;
                Shading::calculateLighting(v, worldNormal, viewDir,
                                           localLights, localLightCount,
                                           baseR, baseG, baseB, r, g, b);

                if (fog.enabled)
                {
                    const float dist = vDistSq * vInvLen;
                    float fogFactor = (dist - fog.worldNear) * fog.worldScale;
                    fogFactor = clamp(fogFactor, 0.0f, 1.0f);
                    const float invFog = 1.0f - fogFactor;
                    r = r * invFog + fog.color_r * fogFactor;
                    g = g * invFog + fog.color_g_f * fogFactor;
                    b = b * invFog + fog.color_b_f * fogFactor;
                }

                vertexColors_[vi] = Vector3(r, g, b);
            }
        }

        const Face *PIP3D_RESTRICT fbase = mesh->faceData();
        for (uint16_t i = 0; i < faceCount; ++i)
        {
            statsTrianglesTotal++;
#if PIP3D_ENABLE_DRAW_TELEMETRY
            g_drawTelemetry.facesTotal++;
#endif

            const Face &face = fbase[i];

            Vector3 v0, v1, v2;

            if (likely(worldVerts))
            {
                v0 = worldVerts[face.v0];
                v1 = worldVerts[face.v1];
                v2 = worldVerts[face.v2];
            }
            else
            {
                v0 = worldTransform.transformNoDiv(localVerts[face.v0]);
                v1 = worldTransform.transformNoDiv(localVerts[face.v1]);
                v2 = worldTransform.transformNoDiv(localVerts[face.v2]);
            }

            const float d0 = (v0.x - camPos.x) * camFwd.x + (v0.y - camPos.y) * camFwd.y + (v0.z - camPos.z) * camFwd.z;
            const float d1 = (v1.x - camPos.x) * camFwd.x + (v1.y - camPos.y) * camFwd.y + (v1.z - camPos.z) * camFwd.z;
            const float d2 = (v2.x - camPos.x) * camFwd.x + (v2.y - camPos.y) * camFwd.y + (v2.z - camPos.z) * camFwd.z;

            if (unlikely(d0 < nearClip && d1 < nearClip && d2 < nearClip))
            {
                statsTrianglesBackfaceCulled++;
#if PIP3D_ENABLE_DRAW_TELEMETRY
                g_drawTelemetry.recordSkip(
                    SkipReason::NEAR_FULLY, currentFrame, i, mesh,
                    d0, d1, d2, 0.0f, 0, 0, 0, 0, 0, 0, 0,
                    v0.x, v0.y, v0.z, camPos.x, camPos.y, camPos.z,
                    camFwd.x, camFwd.y, camFwd.z, nearPlane, bandTop, bandBottom,
                    true, isTextured);
#endif
                continue;
            }

            const bool partiallyClipped = unlikely(d0 < nearClip || d1 < nearClip || d2 < nearClip);

            if (doBackfaceCull)
            {

                const float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
                const float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
                const float nx = e1y * e2z - e1z * e2y;
                const float ny = e1z * e2x - e1x * e2z;
                const float nz = e1x * e2y - e1y * e2x;
                const float vx = v0.x - camPos.x;
                const float vy = v0.y - camPos.y;
                const float vz = v0.z - camPos.z;

                if (nx * vx + ny * vy + nz * vz >= 0.0f)
                {
                    statsTrianglesBackfaceCulled++;
#if PIP3D_ENABLE_DRAW_TELEMETRY
                    g_drawTelemetry.recordSkip(
                        SkipReason::BACKFACE, currentFrame, i, mesh,
                        d0, d1, d2, 0.0f, 0, 0, 0, 0, 0, 0, 0,
                        v0.x, v0.y, v0.z, camPos.x, camPos.y, camPos.z,
                        camFwd.x, camFwd.y, camFwd.z, nearPlane, bandTop, bandBottom,
                        partiallyClipped, isTextured);
#endif
                    continue;
                }
            }

            Vector3 p0, p1, p2;

            if (likely(screenVerts))
            {
                p0 = screenVerts[face.v0];
                p1 = screenVerts[face.v1];
                p2 = screenVerts[face.v2];
            }
            else
            {
                p0 = CameraController::project(v0, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, 0, 0);
                p1 = CameraController::project(v1, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, 0, 0);
                p2 = CameraController::project(v2, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, 0, 0);
            }

            if (!partiallyClipped)
            {
                const float area = (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y);

                if (fabsf(area) <= 1.0f)
                {
                    statsTrianglesBackfaceCulled++;
#if PIP3D_ENABLE_DRAW_TELEMETRY
                    g_drawTelemetry.recordSkip(
                        SkipReason::DEGENERATE, currentFrame, i, mesh,
                        d0, d1, d2, area,
                        p0.x, p0.y, p0.z, p1.x, p1.y, p2.x, p2.y,
                        v0.x, v0.y, v0.z, camPos.x, camPos.y, camPos.z,
                        camFwd.x, camFwd.y, camFwd.z, nearPlane, bandTop, bandBottom,
                        partiallyClipped, isTextured);
#endif
                    continue;
                }
            }

            if (isTextured)
            {
                const Vertex &vert0 = vbase[face.v0];
                const Vertex &vert1 = vbase[face.v1];
                const Vertex &vert2 = vbase[face.v2];

                float lr0, lg0, lb0, lr1, lg1, lb1, lr2, lg2, lb2;
                if (gouraudShading)
                {

                    {
                        const float vdx = camPos.x - v0.x;
                        const float vdy = camPos.y - v0.y;
                        const float vdz = camPos.z - v0.z;
                        const float vdsq = vdx * vdx + vdy * vdy + vdz * vdz;
                        const float vil = (vdsq > 1e-8f) ? FastMath::fastInvSqrt(vdsq) : 0.0f;
                        const Vector3 viewDir0(vdx * vil, vdy * vil, vdz * vil);
                        const Vector3 n0 = nmGouraud.transform(vert0.normal.get());
                        Shading::calculateLighting(v0, n0, viewDir0,
                                                   localLights, localLightCount,
                                                   baseR, baseG, baseB, lr0, lg0, lb0);
                    }
                    {
                        const float vdx = camPos.x - v1.x;
                        const float vdy = camPos.y - v1.y;
                        const float vdz = camPos.z - v1.z;
                        const float vdsq = vdx * vdx + vdy * vdy + vdz * vdz;
                        const float vil = (vdsq > 1e-8f) ? FastMath::fastInvSqrt(vdsq) : 0.0f;
                        const Vector3 viewDir1(vdx * vil, vdy * vil, vdz * vil);
                        const Vector3 n1 = nmGouraud.transform(vert1.normal.get());
                        Shading::calculateLighting(v1, n1, viewDir1,
                                                   localLights, localLightCount,
                                                   baseR, baseG, baseB, lr1, lg1, lb1);
                    }
                    {
                        const float vdx = camPos.x - v2.x;
                        const float vdy = camPos.y - v2.y;
                        const float vdz = camPos.z - v2.z;
                        const float vdsq = vdx * vdx + vdy * vdy + vdz * vdz;
                        const float vil = (vdsq > 1e-8f) ? FastMath::fastInvSqrt(vdsq) : 0.0f;
                        const Vector3 viewDir2(vdx * vil, vdy * vil, vdz * vil);
                        const Vector3 n2 = nmGouraud.transform(vert2.normal.get());
                        Shading::calculateLighting(v2, n2, viewDir2,
                                                   localLights, localLightCount,
                                                   baseR, baseG, baseB, lr2, lg2, lb2);
                    }
                }
                else
                {

                    Shading::calculateFaceLightingWithFog(
                        v0, v1, v2, camPos,
                        localLights, localLightCount,
                        baseR, baseG, baseB,
                        lr0, lg0, lb0);
                    lr1 = lr0;
                    lg1 = lg0;
                    lb1 = lb0;
                    lr2 = lr0;
                    lg2 = lg0;
                    lb2 = lb0;
                }

                if (!partiallyClipped)
                {

                    Rasterizer::fillTriangleTextured(
                        p0.x, p0.y - bandTopF, p0.z,
                        p1.x, p1.y - bandTopF, p1.z,
                        p2.x, p2.y - bandTopF, p2.z,
                        vert0.tu, vert0.tv,
                        vert1.tu, vert1.tv,
                        vert2.tu, vert2.tv,
                        d0, d1, d2,
                        lr0, lg0, lb0,
                        lr1, lg1, lb1,
                        lr2, lg2, lb2,
                        *meshTexture,
                        framebuffer.getBuffer(),
                        &zBuffer,
                        framebufferConfig);
#if PIP3D_ENABLE_DRAW_TELEMETRY
                    g_drawTelemetry.facesDrawnTextured++;
#endif
                }
                else
                {
                    DrawTelemetryClipVert cv[3] = {
                        {v0, vert0.tu, vert0.tv, d0, lr0, lg0, lb0},
                        {v1, vert1.tu, vert1.tv, d1, lr1, lg1, lb1},
                        {v2, vert2.tu, vert2.tv, d2, lr2, lg2, lb2}};
                    const bool drew = clipAndDrawNearTextured(
                        cv, nearClip,
                        cam, viewport, viewProjMatrix,
                        framebuffer, &zBuffer,
                        *meshTexture,
                        mesh, i, currentFrame);
#if PIP3D_ENABLE_DRAW_TELEMETRY
                    if (drew)
                        g_drawTelemetry.facesDrawnClipped++;
                    else
                        g_drawTelemetry.recordSkip(
                            SkipReason::CLIP_OUTCOUNT_LT3, currentFrame, i, mesh,
                            d0, d1, d2, 0.0f, 0, 0, 0, 0, 0, 0, 0,
                            v0.x, v0.y, v0.z, camPos.x, camPos.y, camPos.z,
                            camFwd.x, camFwd.y, camFwd.z, nearPlane, bandTop, bandBottom,
                            true, true);
#endif
                }
                continue;
            }

            if (!partiallyClipped)
            {

                const float minY = (p0.y < p1.y) ? ((p0.y < p2.y) ? p0.y : p2.y)
                                                 : ((p1.y < p2.y) ? p1.y : p2.y);
                const float maxY = (p0.y > p1.y) ? ((p0.y > p2.y) ? p0.y : p2.y)
                                                 : ((p1.y > p2.y) ? p1.y : p2.y);
                if (maxY < bandTop || minY >= bandBottom)
                    continue;

                const float minX = (p0.x < p1.x) ? ((p0.x < p2.x) ? p0.x : p2.x)
                                                 : ((p1.x < p2.x) ? p1.x : p2.x);
                const float maxX = (p0.x > p1.x) ? ((p0.x > p2.x) ? p0.x : p2.x)
                                                 : ((p1.x > p2.x) ? p1.x : p2.x);
                if (maxX < 0.0f || minX >= viewportWidth)
                    continue;
            }

            if (gouraudShading && !partiallyClipped)
            {
                const Vector3 &c0 = vertexColors_[face.v0];
                const Vector3 &c1 = vertexColors_[face.v1];
                const Vector3 &c2 = vertexColors_[face.v2];

                Rasterizer::fillTriangleSmooth(
                    (int16_t)p0.x, (int16_t)(p0.y - bandTopF), p0.z,
                    (int16_t)p1.x, (int16_t)(p1.y - bandTopF), p1.z,
                    (int16_t)p2.x, (int16_t)(p2.y - bandTopF), p2.z,
                    c0.x, c0.y, c0.z, c1.x, c1.y, c1.z, c2.x, c2.y, c2.z,
                    framebuffer.getBuffer(), &zBuffer, framebufferConfig);
                continue;
            }

            MeshRenderer::drawTriangle3D_Preprojected(
                v0, v1, v2, p0, p1, p2,
                d0, d1, d2,
                partiallyClipped,
                nearClip,
                camPos,
                instColor565,
                viewProjMatrix,
                viewport,
                viewportHalfWidth, viewportHalfHeight, viewportWidth,
                bandTop, bandBottom, bandTopF,
                framebuffer, &zBuffer,
                localLights, localLightCount,
                useUniformColor, uniformColor);
        }
    }

    void Renderer::drawTriangle3D(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, uint16_t color)
    {
        MeshRenderer::drawTriangle3D(v0, v1, v2, color, cameras[activeCameraIndex], viewport,
                                     viewProjMatrix, framebuffer, &zBuffer, lights.data(), activeLightCount,
                                     false, 0);
    }
}
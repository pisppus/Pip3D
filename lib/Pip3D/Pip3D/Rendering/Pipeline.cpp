#include "Renderer.hpp"
#include "Pipeline/Culling.hpp"
#include "Rendering/Pipeline/Telemetry.hpp"
#include "Debug/Logging.hpp"
#include "Rendering/Pipeline/MeshDraw.hpp"
#include <vector>

namespace pip3D
{
    static int collectActiveLightsForBounds(const Vector3 &center, float radius,
                                            const Light *allLights, int allLightCount,
                                            Light *outLights, int maxLights)
    {
        int count = 0;

        for (int i = 0; i < allLightCount && count < maxLights; ++i)
        {
            if (allLights[i].type == LIGHT_DIRECTIONAL)
            {
                outLights[count++] = allLights[i];
            }
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
                float dx = l.position.x - center.x;
                float dy = l.position.y - center.y;
                float dz = l.position.z - center.z;
                float distSq = dx * dx + dy * dy + dz * dz;

                float maxDist = l.range + radius;
                if (l.range > 0.0f && distSq > maxDist * maxDist)
                    continue;

                float atten = 1.0f;
                if (l.range > 0.0f)
                {
                    atten = FastMath::fastReciprocal(1.0f + distSq * l.invRangeSq);
                }
                float score = l.intensity * atten;

                scores[scoreCount++] = {i, score};
            }
        }

        for (int i = 1; i < scoreCount; ++i)
        {
            LightScore temp = scores[i];
            int j = i - 1;
            while (j >= 0 && scores[j].score < temp.score)
            {
                scores[j + 1] = scores[j];
                j--;
            }
            scores[j + 1] = temp;
        }

        for (int i = 0; i < scoreCount && count < maxLights; ++i)
        {
            outLights[count++] = allLights[scores[i].index];
        }

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
            if (instance->getBlobShadow() || mesh->getBlobShadow())
            {
                if (blobShadowQueueCount < MAX_QUEUE_ELEMENTS)
                    blobShadowQueue[blobShadowQueueCount++] = instance;
            }
            else if (mesh->getCastShadows())
            {
                if (shadowQueueCount < MAX_QUEUE_ELEMENTS)
                {
                    shadowQueue[shadowQueueCount++] = instance;
                }
            }
        }

        if (opaqueQueueCount < MAX_QUEUE_ELEMENTS)
        {
            opaqueQueue[opaqueQueueCount++] = instance;
        }
    }

    void Renderer::draw(Mesh *mesh)
    {
        if (unlikely(!mesh || !mesh->isVisible()))
            return;

        if (shadowsEnabled)
        {
            if (mesh->getBlobShadow())
            {
                if (meshBlobShadowQueueCount < MAX_QUEUE_ELEMENTS)
                    meshBlobShadowQueue[meshBlobShadowQueueCount++] = mesh;
            }
            else if (mesh->getCastShadows())
            {
                if (meshShadowQueueCount < MAX_QUEUE_ELEMENTS)
                {
                    meshShadowQueue[meshShadowQueueCount++] = mesh;
                }
            }
        }

        if (meshOpaqueQueueCount < MAX_QUEUE_ELEMENTS)
        {
            meshOpaqueQueue[meshOpaqueQueueCount++] = mesh;
        }
    }

    void Renderer::flushQueue()
    {
        const float blobOpacity = shadowSettings.shadowOpacity;

        for (size_t i = 0; i < blobShadowQueueCount; ++i)
        {
            MeshInstance *inst = blobShadowQueue[i];
            drawBlobShadow(inst->pos(), inst->radius(), blobOpacity);
        }

        for (size_t i = 0; i < meshBlobShadowQueueCount; ++i)
        {
            Mesh *m = meshBlobShadowQueue[i];
            drawBlobShadow(m->center(), m->radius(), blobOpacity);
        }

        for (size_t i = 0; i < shadowQueueCount; ++i)
        {
            drawMeshInstanceShadow(shadowQueue[i]);
        }

        for (size_t i = 0; i < meshShadowQueueCount; ++i)
        {
            drawMeshShadow(meshShadowQueue[i]);
        }

        for (size_t i = 0; i < opaqueQueueCount; ++i)
        {
            drawMeshInstanceInternal(opaqueQueue[i], false);
        }

        for (size_t i = 0; i < meshOpaqueQueueCount; ++i)
        {
            drawMesh(meshOpaqueQueue[i]);
        }
    }

    bool Renderer::clipAndDrawNearTextured(const DrawTelemetryClipVert inVerts[3],
                                           float nearD,
                                           const Camera &camera,
                                           const Viewport &viewport,
                                           const Matrix4x4 &viewProjMatrix,
                                           FrameBuffer &framebuffer,
                                           ZBuffer<SCREEN_WIDTH, SCREEN_BAND_HEIGHT> *zBuffer,
                                           const Texture &tex,
                                           const Mesh *meshForTelemetry,
                                           uint16_t faceIdxForTelemetry,
                                           uint32_t frameForTelemetry)
    {
        DrawTelemetryClipVert clipped[4];
        int outCount = 0;

        for (int i = 0; i < 3; ++i)
        {
            int j = (i + 1) % 3;
            const DrawTelemetryClipVert &P0 = inVerts[i];
            const DrawTelemetryClipVert &P1 = inVerts[j];
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

                DrawTelemetryClipVert ip = lerpClipVert(P0, P1, t);

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
            return false;

        const float viewportHalfWidth = static_cast<float>(viewport.width) * 0.5f;
        const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;
        const int16_t bandTop = g_bandOffsetY;
        const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
        const DisplayConfig &framebufferConfig = framebuffer.getConfig();
        const float viewportWidth = static_cast<float>(viewport.width);

        auto projectAndDraw = [&](const DrawTelemetryClipVert &cv0,
                                  const DrawTelemetryClipVert &cv1,
                                  const DrawTelemetryClipVert &cv2) -> bool
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

            if (!isfinite(p0.x) || !isfinite(p0.y) || !isfinite(p0.z) ||
                !isfinite(p1.x) || !isfinite(p1.y) || !isfinite(p1.z) ||
                !isfinite(p2.x) || !isfinite(p2.y) || !isfinite(p2.z))
            {
                g_drawTelemetry.recordSkip(
                    SkipReason::NAN_PROJECT, frameForTelemetry, faceIdxForTelemetry,
                    meshForTelemetry,
                    cv0.d, cv1.d, cv2.d, 0.0f,
                    p0.x, p0.y, p0.z, p1.x, p1.y, p2.x, p2.y,
                    cv0.pos.x, cv0.pos.y, cv0.pos.z,
                    camera.position.x, camera.position.y, camera.position.z,
                    camera.forward().x, camera.forward().y, camera.forward().z,
                    nearD, bandTop, bandBottom,
                    true, true);
                return false;
            }

            float minY = fminf(p0.y, fminf(p1.y, p2.y));
            float maxY = fmaxf(p0.y, fmaxf(p1.y, p2.y));
            if (maxY < bandTop || minY >= bandBottom)
            {
                g_drawTelemetry.recordSkip(
                    SkipReason::BAND_Y, frameForTelemetry, faceIdxForTelemetry,
                    meshForTelemetry,
                    cv0.d, cv1.d, cv2.d, 0.0f,
                    p0.x, p0.y, p0.z, p1.x, p1.y, p2.x, p2.y,
                    cv0.pos.x, cv0.pos.y, cv0.pos.z,
                    camera.position.x, camera.position.y, camera.position.z,
                    camera.forward().x, camera.forward().y, camera.forward().z,
                    nearD, bandTop, bandBottom,
                    true, true);
                return false;
            }

            float minX = fminf(p0.x, fminf(p1.x, p2.x));
            float maxX = fmaxf(p0.x, fmaxf(p1.x, p2.x));
            if (maxX < 0.0f || minX >= viewportWidth)
            {
                g_drawTelemetry.recordSkip(
                    SkipReason::FRUSTUM_X, frameForTelemetry, faceIdxForTelemetry,
                    meshForTelemetry,
                    cv0.d, cv1.d, cv2.d, 0.0f,
                    p0.x, p0.y, p0.z, p1.x, p1.y, p2.x, p2.y,
                    cv0.pos.x, cv0.pos.y, cv0.pos.z,
                    camera.position.x, camera.position.y, camera.position.z,
                    camera.forward().x, camera.forward().y, camera.forward().z,
                    nearD, bandTop, bandBottom,
                    true, true);
                return false;
            }

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
            return true;
        };

        bool drew = projectAndDraw(clipped[0], clipped[1], clipped[2]);
        if (outCount == 4)
            drew |= projectAndDraw(clipped[0], clipped[2], clipped[3]);
        return drew;
    }

    void Renderer::drawMeshInstanceInternal(MeshInstance *instance, bool performFrustumCull)
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
        if (cam.projectionType == PERSPECTIVE)
        {
            Vector3 toCenter = center - cam.position;
            float distForward = toCenter.dot(cam.forward());
            if (distForward > cam.nearPlane)
            {
                static float cachedFov = -1.0f;
                static float cachedProjScale = 1.0f;
                if (unlikely(cam.fov != cachedFov))
                {
                    cachedFov = cam.fov;
                    float s, c;
                    FastMath::fastSinCos(cam.fov * 0.5f * kDegToRad, s, c);
                    cachedProjScale = c * FastMath::fastReciprocal(s);
                }

                const float invDist = FastMath::fastReciprocal(distForward);
                const float radiusPixels = radius * cachedProjScale * invDist * (static_cast<float>(viewport.height) * 0.5f);

                if (radiusPixels < 1.0f)
                {
                    statsInstancesTotal++;
                    return;
                }
            }
        }

        statsInstancesTotal++;

        const DisplayConfig &framebufferConfig = framebuffer.getConfig();
        if (occlusionCullingEnabled &&
            Culling::isInstanceOccluded(center, radius, cam, viewport, viewProjMatrix, zBuffer, framebufferConfig))
        {
            statsInstancesOcclusionCulled++;
            return;
        }

        const uint16_t instColor565 = instance->color().rgb565;
        float baseR, baseG, baseB;
        MeshRenderer::decodeColorToFloat(instColor565, baseR, baseG, baseB);

        bool useUniformColor = mesh->getSingleColorLighting();
        uint16_t uniformColor = 0;

        Light localLights[4];
        int localLightCount = collectActiveLightsForBounds(center, radius, lights.data(), activeLightCount, localLights, 4);
        const Light *const activeLights = localLights;
        const int actualLightCount = localLightCount;

        const Matrix4x4 &worldTransform = instance->transform();

        if (useUniformColor)
        {
            Vector3 localNormal = mesh->numVertices() > 0 ? mesh->vert(0).normal.get() : Vector3(0.0f, 1.0f, 0.0f);
            Vector3 worldNormal = worldTransform.transformNormal(localNormal);
            Vector3 viewDir = cam.position - center;
            const float viewDistSq = viewDir.lengthSquared();
            viewDir.normalize();

            float finalR, finalG, finalB;
            Shading::calculateLighting(center, worldNormal, viewDir, activeLights, actualLightCount, baseR, baseG, baseB, finalR, finalG, finalB, true);

            if (Rasterizer::g_fogState.enabled)
            {
                const float dist = sqrtf(viewDistSq);
                float fogFactor = (dist - Rasterizer::g_fogState.worldNear) * Rasterizer::g_fogState.worldScale;
                fogFactor = fminf(fogFactor, 1.0f);
                fogFactor = fmaxf(fogFactor, 0.0f);

                const float invFog = 1.0f - fogFactor;
                finalR = finalR * invFog + Rasterizer::g_fogState.color_r * fogFactor;
                finalG = finalG * invFog + Rasterizer::g_fogState.color_g_f * fogFactor;
                finalB = finalB * invFog + Rasterizer::g_fogState.color_b_f * fogFactor;
            }

            uniformColor = Color::fromFloat(finalR, finalG, finalB).rgb565;
        }

        const uint16_t vertexCountUsed = mesh->numVertices();
        const uint16_t faceCount = mesh->numFaces();

        bool useFallbackPath = false;
        Vector3 *worldVerts = nullptr;
        Vector3 *screenVerts = nullptr;

        if (instance->ensureProjectionCache(vertexCountUsed))
        {
            worldVerts = instance->getCachedWorldVertices();
            screenVerts = instance->getCachedScreenVertices();
        }
        else
        {
            useFallbackPath = true;
        }

        const Vector3 *localVerts = nullptr;
        if (mesh->ensureDecodedVertexCache())
            localVerts = mesh->getCachedLocalVertices();

        const uint32_t frameStamp = g_frameStamp;
        const int16_t bandTop = g_bandOffsetY;
        const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
        const float viewportWidth = static_cast<float>(viewport.width);
        const float viewportHalfWidth = viewportWidth * 0.5f;
        const float viewportHalfHeight = static_cast<float>(viewport.height) * 0.5f;

        if (!useFallbackPath && instance->getCachedProjectionFrameStamp() != frameStamp)
        {
            for (uint16_t i = 0; i < vertexCountUsed; ++i)
            {
                Vector3 local = localVerts ? localVerts[i] : mesh->decodePosition(mesh->vert(i));
                Vector3 world = worldTransform.transformNoDiv(local);
                worldVerts[i] = world;
                screenVerts[i] = CameraController::project(world, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
            }

            instance->setCachedProjectionFrameStamp(frameStamp);
        }

        thread_local static std::vector<Vector3> vertexColors;
        if (shadingMode == SHADING_GOURAUD && !useUniformColor)
        {
            if (vertexColors.size() < vertexCountUsed)
                vertexColors.resize(vertexCountUsed);

            const Vector3 camPos = cam.position;
            for (uint16_t vi = 0; vi < vertexCountUsed; ++vi)
            {
                Vector3 localNormal = mesh->vert(vi).normal.get();
                Vector3 worldNormal = worldTransform.transformNormal(localNormal);
                Vector3 v;
                if (!useFallbackPath)
                {
                    v = worldVerts[vi];
                }
                else
                {
                    Vector3 local = localVerts ? localVerts[vi] : mesh->decodePosition(mesh->vert(vi));
                    v = worldTransform.transformNoDiv(local);
                }
                Vector3 viewDir = camPos - v;
                const float viewDistSq = viewDir.lengthSquared();
                viewDir.normalize();

                float r, g, b;
                Shading::calculateLighting(v, worldNormal, viewDir, activeLights, actualLightCount, baseR, baseG, baseB, r, g, b);

                if (Rasterizer::g_fogState.enabled)
                {
                    const float dist = sqrtf(viewDistSq);
                    float fogFactor = (dist - Rasterizer::g_fogState.worldNear) * Rasterizer::g_fogState.worldScale;
                    fogFactor = fminf(fogFactor, 1.0f);
                    fogFactor = fmaxf(fogFactor, 0.0f);

                    const float invFog = 1.0f - fogFactor;
                    r = r * invFog + Rasterizer::g_fogState.color_r * fogFactor;
                    g = g * invFog + Rasterizer::g_fogState.color_g_f * fogFactor;
                    b = b * invFog + Rasterizer::g_fogState.color_b_f * fogFactor;
                }

                vertexColors[vi] = Vector3(r, g, b);
            }
        }

        const Vector3 &camFwd = cam.forward();
        const Vector3 camPos = cam.position;
        const float nearPlane = cam.nearPlane;
        const bool usePerspectiveFacing = cam.projectionType == PERSPECTIVE;
        const bool isTextured = mesh->isTextured();
        const bool doBackfaceCull = backfaceCullingEnabled;
        const bool gouraudShading = (shadingMode == SHADING_GOURAUD) && !useUniformColor;
        const uint32_t currentFrame = g_frameStamp;

        constexpr float kNearClipEps = 1e-4f;
        const float nearClip = nearPlane + kNearClipEps;

        for (uint16_t i = 0; i < faceCount; ++i)
        {
            statsTrianglesTotal++;
            g_drawTelemetry.facesTotal++;

            const Face &face = mesh->face(i);

            Vector3 v0, v1, v2;

            if (!useFallbackPath)
            {
                v0 = worldVerts[face.v0];
                v1 = worldVerts[face.v1];
                v2 = worldVerts[face.v2];
            }
            else
            {
                Vector3 local0 = localVerts ? localVerts[face.v0] : mesh->decodePosition(mesh->vert(face.v0));
                Vector3 local1 = localVerts ? localVerts[face.v1] : mesh->decodePosition(mesh->vert(face.v1));
                Vector3 local2 = localVerts ? localVerts[face.v2] : mesh->decodePosition(mesh->vert(face.v2));

                v0 = worldTransform.transformNoDiv(local0);
                v1 = worldTransform.transformNoDiv(local1);
                v2 = worldTransform.transformNoDiv(local2);
            }

            const Vector3 toCam0 = camPos - v0;

            const float d0 = -toCam0.dot(camFwd);
            const float d1 = (v1 - camPos).dot(camFwd);
            const float d2 = (v2 - camPos).dot(camFwd);

            if (d0 < nearClip && d1 < nearClip && d2 < nearClip)
            {
                statsTrianglesBackfaceCulled++;
                g_drawTelemetry.recordSkip(
                    SkipReason::NEAR_FULLY, currentFrame, i, mesh,
                    d0, d1, d2, 0.0f,
                    0, 0, 0, 0, 0, 0, 0,
                    v0.x, v0.y, v0.z,
                    camPos.x, camPos.y, camPos.z,
                    camFwd.x, camFwd.y, camFwd.z,
                    nearPlane, bandTop, bandBottom,
                    true, isTextured);
                continue;
            }

            const bool partiallyClipped = (d0 < nearClip || d1 < nearClip || d2 < nearClip);

            if (doBackfaceCull)
            {
                float nx = (v1.y - v0.y) * (v2.z - v0.z) - (v1.z - v0.z) * (v2.y - v0.y);
                float ny = (v1.z - v0.z) * (v2.x - v0.x) - (v1.x - v0.x) * (v2.z - v0.z);
                float nz = (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x);
                float vx = v0.x - camPos.x;
                float vy = v0.y - camPos.y;
                float vz = v0.z - camPos.z;

                if (nx * vx + ny * vy + nz * vz >= 0.0f)
                {
                    statsTrianglesBackfaceCulled++;
                    g_drawTelemetry.recordSkip(
                        SkipReason::BACKFACE, currentFrame, i, mesh,
                        d0, d1, d2, 0.0f,
                        0, 0, 0, 0, 0, 0, 0,
                        v0.x, v0.y, v0.z,
                        camPos.x, camPos.y, camPos.z,
                        camFwd.x, camFwd.y, camFwd.z,
                        nearPlane, bandTop, bandBottom,
                        partiallyClipped, isTextured);
                    continue;
                }
            }

            Vector3 p0, p1, p2;

            if (!useFallbackPath)
            {
                p0 = screenVerts[face.v0];
                p1 = screenVerts[face.v1];
                p2 = screenVerts[face.v2];
            }
            else
            {
                p0 = CameraController::project(v0, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
                p1 = CameraController::project(v1, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
                p2 = CameraController::project(v2, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
            }

            if (!partiallyClipped)
            {
                const float area = (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y);

                if (fabsf(area) <= 1.0f)
                {
                    statsTrianglesBackfaceCulled++;
                    g_drawTelemetry.recordSkip(
                        SkipReason::DEGENERATE, currentFrame, i, mesh,
                        d0, d1, d2, area,
                        p0.x, p0.y, p0.z, p1.x, p1.y, p2.x, p2.y,
                        v0.x, v0.y, v0.z,
                        camPos.x, camPos.y, camPos.z,
                        camFwd.x, camFwd.y, camFwd.z,
                        nearPlane, bandTop, bandBottom,
                        partiallyClipped, isTextured);
                    continue;
                }
            }

            if (isTextured)
            {
                const Vertex &vert0 = mesh->vert(face.v0);
                const Vertex &vert1 = mesh->vert(face.v1);
                const Vertex &vert2 = mesh->vert(face.v2);

                float lr0, lg0, lb0, lr1, lg1, lb1, lr2, lg2, lb2;
                if (gouraudShading)
                {
                    Vector3 viewDir0 = camPos - v0;
                    viewDir0.normalize();
                    Vector3 n0 = worldTransform.transformNormal(vert0.normal.get());
                    Shading::calculateLighting(v0, n0, viewDir0, activeLights, actualLightCount, baseR, baseG, baseB, lr0, lg0, lb0);

                    Vector3 viewDir1 = camPos - v1;
                    viewDir1.normalize();
                    Vector3 n1 = worldTransform.transformNormal(vert1.normal.get());
                    Shading::calculateLighting(v1, n1, viewDir1, activeLights, actualLightCount, baseR, baseG, baseB, lr1, lg1, lb1);

                    Vector3 viewDir2 = camPos - v2;
                    viewDir2.normalize();
                    Vector3 n2 = worldTransform.transformNormal(vert2.normal.get());
                    Shading::calculateLighting(v2, n2, viewDir2, activeLights, actualLightCount, baseR, baseG, baseB, lr2, lg2, lb2);
                }
                else
                {
                    Vector3 faceNormal = (v1 - v0).cross(v2 - v0);
                    faceNormal.normalize();
                    const Vector3 centroid = (v0 + v1 + v2) * (1.0f / 3.0f);
                    Vector3 viewDir = camPos - centroid;
                    viewDir.normalize();
                    Shading::calculateLighting(centroid, faceNormal, viewDir, activeLights, actualLightCount, baseR, baseG, baseB, lr0, lg0, lb0);
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
                    g_drawTelemetry.facesDrawnTextured++;
                }
                else
                {
                    DrawTelemetryClipVert cv[3] = {
                        {v0, vert0.tu, vert0.tv, d0, lr0, lg0, lb0},
                        {v1, vert1.tu, vert1.tv, d1, lr1, lg1, lb1},
                        {v2, vert2.tu, vert2.tv, d2, lr2, lg2, lb2}};
                    bool drew = clipAndDrawNearTextured(
                        cv, nearClip,
                        cam, viewport, viewProjMatrix,
                        framebuffer, zBuffer,
                        *mesh->getTexture(),
                        mesh, i, currentFrame);
                    if (drew)
                        g_drawTelemetry.facesDrawnClipped++;
                    else
                        g_drawTelemetry.recordSkip(
                            SkipReason::CLIP_OUTCOUNT_LT3, currentFrame, i, mesh,
                            d0, d1, d2, 0.0f,
                            0, 0, 0, 0, 0, 0, 0,
                            v0.x, v0.y, v0.z,
                            camPos.x, camPos.y, camPos.z,
                            camFwd.x, camFwd.y, camFwd.z,
                            nearPlane, bandTop, bandBottom,
                            true, true);
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

            if (gouraudShading && !partiallyClipped)
            {
                const Vector3 &c0 = vertexColors[face.v0];
                const Vector3 &c1 = vertexColors[face.v1];
                const Vector3 &c2 = vertexColors[face.v2];

                Vector3 lp0 = p0;
                Vector3 lp1 = p1;
                Vector3 lp2 = p2;
                lp0.y -= (float)bandTop;
                lp1.y -= (float)bandTop;
                lp2.y -= (float)bandTop;

                Rasterizer::fillTriangleSmooth((int16_t)lp0.x, (int16_t)lp0.y, lp0.z, (int16_t)lp1.x, (int16_t)lp1.y, lp1.z, (int16_t)lp2.x, (int16_t)lp2.y, lp2.z, c0.x, c0.y, c0.z, c1.x, c1.y, c1.z, c2.x, c2.y, c2.z, framebuffer.getBuffer(), zBuffer, framebufferConfig);
                continue;
            }

            MeshRenderer::drawTriangle3D_Preprojected(v0, v1, v2, p0, p1, p2, instColor565, cam, viewport, viewProjMatrix, framebuffer, zBuffer, activeLights, actualLightCount, backfaceCullingEnabled, statsTrianglesTotal, statsTrianglesBackfaceCulled, useUniformColor, uniformColor);
        }
    }

    void Renderer::drawMeshInstance(MeshInstance *instance)
    {
        drawMeshInstanceInternal(instance, true);
    }

    void Renderer::drawMeshInstance(MeshInstance *instance, ShadingMode mode)
    {
        ShadingMode prev = shadingMode;
        shadingMode = mode;
        drawMeshInstance(instance);
        shadingMode = prev;
    }

    void Renderer::drawMeshInstanceStatic(MeshInstance *instance)
    {
        drawMeshInstanceInternal(instance, true);
    }

    void Renderer::drawInstances(InstanceManager &manager)
    {
        static std::vector<MeshInstance *> visibleInstances;
        visibleInstances.clear();
        visibleInstances.reserve(manager.count());
        manager.cull(frustum, visibleInstances);

        manager.sort(cameras[activeCameraIndex].position, visibleInstances);

        for (auto *instance : visibleInstances)
        {
            drawMeshInstanceInternal(instance, false);
        }
    }

    void Renderer::drawMesh(Mesh *mesh)
    {
        if (!mesh || !mesh->isVisible())
            return;

        Vector3 center = mesh->center();
        float radius = mesh->radius();

        Light localLights[4];
        int localLightCount = collectActiveLightsForBounds(center, radius, lights.data(), activeLightCount, localLights, 4);

        MeshRenderer::drawMesh(mesh, cameras[activeCameraIndex], viewport, frustum, viewProjMatrix, framebuffer, zBuffer, localLights, localLightCount, backfaceCullingEnabled, statsTrianglesTotal, statsTrianglesBackfaceCulled, shadingMode);
    }

    void Renderer::drawMesh(Mesh *mesh, ShadingMode mode)
    {
        ShadingMode prev = shadingMode;
        shadingMode = mode;
        drawMesh(mesh);
        shadingMode = prev;
    }

    void Renderer::drawTriangle3D(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, uint16_t color)
    {
        MeshRenderer::drawTriangle3D(v0, v1, v2, color, cameras[activeCameraIndex], viewport, viewProjMatrix, framebuffer, zBuffer, lights.data(), activeLightCount, backfaceCullingEnabled, statsTrianglesTotal, statsTrianglesBackfaceCulled, false, 0);
    }
}
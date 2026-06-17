#include "Renderer.hpp"
#include "Pipeline/Culling.hpp"
#include "Rendering/Pipeline/MeshDraw.hpp"
#include "Debug/Logging.hpp"
#include <vector>

namespace pip3D
{
    void Renderer::draw(MeshInstance *instance)
    {
        if (unlikely(!instance || !instance->isVisible()))
            return;

        Mesh *mesh = instance->getMesh();
        if (!mesh)
            return;

        if (shadowsEnabled && mesh->getCastShadows())
        {
            if (shadowQueueCount < MAX_QUEUE_ELEMENTS)
            {
                shadowQueue[shadowQueueCount++] = instance;
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

        if (shadowsEnabled && mesh->getCastShadows())
        {
            if (meshShadowQueueCount < MAX_QUEUE_ELEMENTS)
            {
                meshShadowQueue[meshShadowQueueCount++] = mesh;
            }
        }

        if (meshOpaqueQueueCount < MAX_QUEUE_ELEMENTS)
        {
            meshOpaqueQueue[meshOpaqueQueueCount++] = mesh;
        }
    }

    void Renderer::flushQueue()
    {
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
            drawMeshInstanceInternal(opaqueQueue[i], false, true);
        }

        for (size_t i = 0; i < meshOpaqueQueueCount; ++i)
        {
            drawMesh(meshOpaqueQueue[i]);
        }
    }

    void Renderer::drawMeshInstanceInternal(MeshInstance *instance, bool performFrustumCull, bool trackDirty)
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
            if (!frustum.sphere(center, radius))
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

        if (trackDirty)
        {
            addDirtyFromSphere(instance, center, radius);
        }

        const uint16_t instColor565 = instance->color().rgb565;
        float baseR, baseG, baseB;
        MeshRenderer::decodeColorToFloat(instColor565, baseR, baseG, baseB);

        bool useUniformColor = mesh->getSingleColorLighting();
        uint16_t uniformColor = 0;
        const Light *const activeLights = lights.data();
        const Matrix4x4 &worldTransform = instance->transform();

        if (useUniformColor)
        {
            Vector3 localNormal = mesh->numVertices() > 0 ? mesh->vert(0).normal.get() : Vector3(0.0f, 1.0f, 0.0f);
            Vector3 worldNormal = worldTransform.transformNormal(localNormal);
            Vector3 viewDir = cam.position - center;
            viewDir.normalize();

            float finalR, finalG, finalB;
            Shading::calculateLighting(center, worldNormal, viewDir, activeLights, activeLightCount, baseR, baseG, baseB, finalR, finalG, finalB, true);
            uniformColor = Shading::quantizeColor(finalR, finalG, finalB);
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

        const uint32_t frameStamp = currentFrameStamp();
        const int16_t bandTop = currentBandOffsetY();
        const int16_t bandBottom = static_cast<int16_t>(bandTop + currentBandHeight());
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
                viewDir.normalize();

                float r, g, b;
                Shading::calculateLighting(v, worldNormal, viewDir, activeLights, activeLightCount, baseR, baseG, baseB, r, g, b);
                vertexColors[vi] = Vector3(r, g, b);
            }
        }

        for (uint16_t i = 0; i < faceCount; ++i)
        {
            const Face &face = mesh->face(i);

            Vector3 v0, v1, v2;
            Vector3 p0, p1, p2;

            if (!useFallbackPath)
            {
                worldVerts = instance->getCachedWorldVertices();
                screenVerts = instance->getCachedScreenVertices();

                v0 = worldVerts[face.v0];
                v1 = worldVerts[face.v1];
                v2 = worldVerts[face.v2];

                p0 = screenVerts[face.v0];
                p1 = screenVerts[face.v1];
                p2 = screenVerts[face.v2];
            }
            else
            {
                Vector3 local0 = localVerts ? localVerts[face.v0] : mesh->decodePosition(mesh->vert(face.v0));
                Vector3 local1 = localVerts ? localVerts[face.v1] : mesh->decodePosition(mesh->vert(face.v1));
                Vector3 local2 = localVerts ? localVerts[face.v2] : mesh->decodePosition(mesh->vert(face.v2));

                v0 = worldTransform.transformNoDiv(local0);
                v1 = worldTransform.transformNoDiv(local1);
                v2 = worldTransform.transformNoDiv(local2);

                p0 = CameraController::project(v0, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
                p1 = CameraController::project(v1, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
                p2 = CameraController::project(v2, viewProjMatrix, viewportHalfWidth, viewportHalfHeight, viewport.x, viewport.y);
            }

            float d0 = (v0 - cam.position).dot(cam.forward());
            float d1 = (v1 - cam.position).dot(cam.forward());
            float d2 = (v2 - cam.position).dot(cam.forward());

            if (d0 < cam.nearPlane && d1 < cam.nearPlane && d2 < cam.nearPlane)
                continue;

            bool partiallyClipped = (d0 < cam.nearPlane || d1 < cam.nearPlane || d2 < cam.nearPlane);

            if (mesh->isTextured() && !partiallyClipped)
            {
                const Vertex &vert0 = mesh->vert(face.v0);
                const Vertex &vert1 = mesh->vert(face.v1);
                const Vertex &vert2 = mesh->vert(face.v2);

                Vector3 lp0 = p0;
                Vector3 lp1 = p1;
                Vector3 lp2 = p2;
                lp0.y -= (float)bandTop;
                lp1.y -= (float)bandTop;
                lp2.y -= (float)bandTop;

                Rasterizer::fillTriangleTextured(lp0.x, lp0.y, lp0.z, lp1.x, lp1.y, lp1.z, lp2.x, lp2.y, lp2.z, vert0.tu, vert0.tv, vert1.tu, vert1.tv, vert2.tu, vert2.tv, d0, d1, d2, *mesh->getTexture(), framebuffer.getBuffer(), zBuffer, framebufferConfig);
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

            statsTrianglesTotal++;
            if (backfaceCullingEnabled)
            {
                Vector3 faceNormal = (v1 - v0).cross(v2 - v0);
                float normalLenSq = faceNormal.lengthSquared();
                if (normalLenSq <= 1e-10f)
                {
                    statsTrianglesBackfaceCulled++;
                    continue;
                }

                float facing = faceNormal.dot(cam.position - v0);
                if (facing <= 0.0f)
                {
                    statsTrianglesBackfaceCulled++;
                    continue;
                }
            }

            if (shadingMode == SHADING_GOURAUD && !useUniformColor && !partiallyClipped)
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

            MeshRenderer::drawTriangle3D_Preprojected(v0, v1, v2, p0, p1, p2, instColor565, cam, viewport, viewProjMatrix, framebuffer, zBuffer, activeLights, activeLightCount, backfaceCullingEnabled, statsTrianglesTotal, statsTrianglesBackfaceCulled, useUniformColor, uniformColor);
        }
    }

    void Renderer::drawMeshInstance(MeshInstance *instance)
    {
        drawMeshInstanceInternal(instance, true, true);
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
        drawMeshInstanceInternal(instance, true, false);
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
            drawMeshInstanceInternal(instance, false, true);
        }
    }

    void Renderer::drawMesh(Mesh *mesh)
    {
        MeshRenderer::drawMesh(mesh, cameras[activeCameraIndex], viewport, frustum, viewProjMatrix, framebuffer, zBuffer, lights.data(), activeLightCount, backfaceCullingEnabled, statsTrianglesTotal, statsTrianglesBackfaceCulled, shadingMode);
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
        MeshRenderer::drawTriangle3D(v0, v1, v2, color, cameras[activeCameraIndex], viewport, viewProjMatrix, framebuffer, zBuffer, lights.data(), activeLightCount, backfaceCullingEnabled, statsTrianglesTotal, statsTrianglesBackfaceCulled);
    }
}
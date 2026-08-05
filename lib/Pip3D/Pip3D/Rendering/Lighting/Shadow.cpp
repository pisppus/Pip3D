#include "Rendering/Renderer.hpp"
#include "Physics/Physics.hpp"
#include "Rendering/Lighting/Shadow.hpp"

namespace pip3D
{
    bool Renderer::shouldRenderShadowForBounds(const Vector3 &center, float radius) const
    {
        if (!shadowsEnabled || radius <= 0.0f)
            return false;

        const Camera &cam = cameras[activeCameraIndex];

        Vector3 toCenter = center - cam.position;
        float distSq = toCenter.lengthSquared();
        if (distSq <= 1e-6f)
            return true;

        const float maxDistance = cam.farPlane + radius * 6.0f + 24.0f;
        if (distSq > maxDistance * maxDistance)
            return false;

        return true;
    }

    IRAM_ATTR void Renderer::drawMeshInstanceShadow(MeshInstance *instance)
    {
        if (!instance || !instance->isVisible())
            return;
        Mesh *mesh = instance->getMesh();
        if (!mesh || !mesh->getCastShadows())
            return;
        if (!shouldRenderShadowForBounds(instance->center(), instance->radius()))
            return;

        Mesh *shadowMesh = mesh;

        ShadowSettings activeSettings = shadowSettings;
        uint32_t cacheGen = shadowCacheGeneration;
        if (physicsWorld)
        {
            RaycastHit hit;
            Ray downRay(instance->pos(), Vector3(0.0f, -1.0f, 0.0f));
            if (physicsWorld->raycast(downRay, hit, 30.0f))
            {
                Vector3 hitPointWithOffset = hit.point + hit.normal * activeSettings.shadowOffset;
                activeSettings.plane = ShadowProjector::ShadowPlane::fromPointAndNormal(hitPointWithOffset, hit.normal);
                cacheGen = 0;
            }
        }

        DrawCache *cache = getDrawCache(instance);

        ShadowRenderer::drawMeshInstanceShadow(instance,
                                               shadowMesh,
                                               shadowsEnabled,
                                               activeSettings,
                                               cameras[activeCameraIndex],
                                               lights.data(),
                                               activeLightCount,
                                               viewProjMatrix,
                                               viewport,
                                               framebuffer,
                                               &zBuffer,
                                               backfaceCullingEnabled,
                                               cache,
                                               cacheGen);
    }

    void Renderer::drawBlobShadow(const Vector3 &pos, float radius, float opacity)
    {
        if (!shadowsEnabled || opacity <= 0.0f || radius <= 0.0f)
            return;

        float groundY = 0.0f;

        if (physicsWorld)
        {
            RaycastHit hit;
            Ray downRay(pos, Vector3(0.0f, -1.0f, 0.0f));
            if (physicsWorld->raycast(downRay, hit, 30.0f))
            {
                groundY = hit.point.y + 0.002f;
            }
            else
            {
                groundY = pos.y;
            }
        }
        else
        {

            const float planeY = -shadowSettings.plane.d / shadowSettings.plane.normal.y;
            groundY = (shadowSettings.plane.normal.y != 0.0f) ? planeY : 0.0f;
        }

        float heightDiff = pos.y - groundY;
        if (heightDiff < 0.0f)
            heightDiff = 0.0f;

        float maxDist = radius * 4.0f;
        float heightFade = 1.0f - (heightDiff / maxDist);
        if (heightFade <= 0.0f)
            return;

        float finalOpacity = opacity * heightFade;
        uint8_t baseAlpha = static_cast<uint8_t>(finalOpacity * COLOR_BYTE_MAX_F);
        if (baseAlpha == 0)
            return;

        uint16_t srcColor = shadowSettings.shadowColor.rgb565;
        uint16_t r = (uint16_t)((srcColor >> 11) & 0x1F);
        uint16_t g = (uint16_t)((srcColor >> 5) & 0x3F);
        uint16_t b = (uint16_t)(srcColor & 0x1F);
        uint16_t finalShadowColor = (uint16_t)((r << 11) | (g << 5) | b);

        Vector3 v0(pos.x - radius, groundY, pos.z - radius);
        Vector3 v1(pos.x + radius, groundY, pos.z - radius);
        Vector3 v2(pos.x + radius, groundY, pos.z + radius);
        Vector3 v3(pos.x - radius, groundY, pos.z + radius);

        Vector3 p0 = CameraController::project(v0, viewProjMatrix, viewport);
        Vector3 p1 = CameraController::project(v1, viewProjMatrix, viewport);
        Vector3 p2 = CameraController::project(v2, viewProjMatrix, viewport);
        Vector3 p3 = CameraController::project(v3, viewProjMatrix, viewport);

        const Camera &cam = cameras[activeCameraIndex];

        {
            const Vector3 camFwd = cam.forward();
            const float d0 = (v0 - cam.position).dot(camFwd);
            const float d1 = (v1 - cam.position).dot(camFwd);
            const float d2 = (v2 - cam.position).dot(camFwd);
            const float d3 = (v3 - cam.position).dot(camFwd);
            if (d0 < cam.nearPlane && d1 < cam.nearPlane &&
                d2 < cam.nearPlane && d3 < cam.nearPlane)
            {

                return;
            }
        }

        const int16_t bandTop = g_bandOffsetY;
        const int16_t bandBottom = static_cast<int16_t>(bandTop + g_bandHeight);
        const DisplayConfig &cfg = framebuffer.getConfig();

        float minY = fminf(p0.y, fminf(p1.y, fminf(p2.y, p3.y)));
        float maxY = fmaxf(p0.y, fmaxf(p1.y, fmaxf(p2.y, p3.y)));
        if (maxY < bandTop || minY >= bandBottom)
            return;

        p0.y -= (float)bandTop;
        p1.y -= (float)bandTop;
        p2.y -= (float)bandTop;
        p3.y -= (float)bandTop;

        Rasterizer::fillTriangleBlob(
            p0.x, p0.y, p0.z,
            p1.x, p1.y, p1.z,
            p2.x, p2.y, p2.z,
            -1.0f, -1.0f,
            1.0f, -1.0f,
            1.0f, 1.0f,
            finalShadowColor, baseAlpha,
            framebuffer.getBuffer(), &zBuffer, cfg);

        Rasterizer::fillTriangleBlob(
            p0.x, p0.y, p0.z,
            p2.x, p2.y, p2.z,
            p3.x, p3.y, p3.z,
            -1.0f, -1.0f,
            1.0f, 1.0f,
            -1.0f, 1.0f,
            finalShadowColor, baseAlpha,
            framebuffer.getBuffer(), &zBuffer, cfg);
    }
}

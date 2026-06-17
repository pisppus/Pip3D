#include "Renderer.hpp"
#include "Physics/Physics.hpp"
#include "Rendering/Lighting/Shadow.hpp"

namespace pip3D
{
    bool Renderer::shouldRenderShadowForBounds(const Vector3 &center, float radius) const
    {
        if (!shadowsEnabled || radius <= 0.0f)
            return false;

        const Camera &cam = cameras[activeCameraIndex];
        if (cam.projectionType != PERSPECTIVE)
            return true;

        Vector3 toCenter = center - cam.position;
        float distSq = toCenter.lengthSquared();
        if (distSq <= 1e-6f)
            return true;

        const float maxDistance = cam.farPlane + radius * 6.0f + 24.0f;
        if (distSq > maxDistance * maxDistance)
            return false;

        return true;
    }

    void Renderer::drawMeshShadow(Mesh *mesh)
    {
        if (!mesh || !mesh->getCastShadows())
            return;
        if (!shouldRenderShadowForBounds(mesh->center(), mesh->radius()))
            return;

        ShadowRenderer::drawMeshShadow(mesh,
                                       shadowsEnabled,
                                       shadowSettings,
                                       cameras[activeCameraIndex],
                                       lights.data(),
                                       activeLightCount,
                                       viewProjMatrix,
                                       viewport,
                                       framebuffer,
                                       zBuffer,
                                       backfaceCullingEnabled);
    }

    void Renderer::drawMeshInstanceShadow(MeshInstance *instance)
    {
        if (!instance || !instance->isVisible())
            return;
        Mesh *mesh = instance->getMesh();
        if (!mesh || !mesh->getCastShadows())
            return;
        if (!shouldRenderShadowForBounds(instance->center(), instance->radius()))
            return;

        ShadowSettings activeSettings = shadowSettings;
        if (physicsWorld)
        {
            RaycastHit hit;
            Ray downRay(instance->pos(), Vector3(0.0f, -1.0f, 0.0f));
            if (physicsWorld->raycast(downRay, hit, 30.0f))
            {
                Vector3 hitPointWithOffset = hit.point + hit.normal * activeSettings.shadowOffset;
                activeSettings.plane = ShadowProjector::ShadowPlane::fromPointAndNormal(hitPointWithOffset, hit.normal);
            }
        }

        ShadowRenderer::drawMeshInstanceShadow(instance,
                                               shadowsEnabled,
                                               activeSettings,
                                               cameras[activeCameraIndex],
                                               lights.data(),
                                               activeLightCount,
                                               viewProjMatrix,
                                               viewport,
                                               framebuffer,
                                               zBuffer,
                                               backfaceCullingEnabled);
    }
}
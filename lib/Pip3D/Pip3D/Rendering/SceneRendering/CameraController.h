#pragma once

#include "Core/Core.h"
#include "Core/Camera.h"
#include "Core/Frustum.h"
#include "Math/Math.h"

namespace pip3D
{
    class CameraController
    {
    public:
        static Vector3 project(const Vector3 &v,
                               const Matrix4x4 &viewProjMatrix,
                               float halfWidth,
                               float halfHeight,
                               int16_t viewportX,
                               int16_t viewportY)
        {
            const float clipX = viewProjMatrix.m[0] * v.x + viewProjMatrix.m[4] * v.y + viewProjMatrix.m[8] * v.z + viewProjMatrix.m[12];
            const float clipY = viewProjMatrix.m[1] * v.x + viewProjMatrix.m[5] * v.y + viewProjMatrix.m[9] * v.z + viewProjMatrix.m[13];
            const float clipZ = viewProjMatrix.m[2] * v.x + viewProjMatrix.m[6] * v.y + viewProjMatrix.m[10] * v.z + viewProjMatrix.m[14];
            const float clipW = viewProjMatrix.m[3] * v.x + viewProjMatrix.m[7] * v.y + viewProjMatrix.m[11] * v.z + viewProjMatrix.m[15];
            const float invW = 1.0f / clipW;

            return Vector3(
                (clipX * invW + 1.0f) * halfWidth + viewportX,
                (1.0f - clipY * invW) * halfHeight + viewportY,
                clipZ * invW);
        }

        static void updateViewProjectionIfNeeded(Camera &camera,
                                                 const Viewport &viewport,
                                                 Matrix4x4 &viewMatrix,
                                                 Matrix4x4 &projMatrix,
                                                 Matrix4x4 &viewProjMatrix,
                                                 Frustum &frustum,
                                                 bool &viewProjMatrixDirty,
                                                 bool &cameraChangedThisFrame)
        {
            if (viewProjMatrixDirty || camera.cache.flags.viewDirty || camera.cache.flags.projDirty)
            {
                cameraChangedThisFrame = true;
                float aspect = (float)viewport.width / viewport.height;
                viewMatrix = camera.getViewMatrix();
                projMatrix = camera.getProjectionMatrix(aspect);
                viewProjMatrix = projMatrix * viewMatrix;
                viewProjMatrixDirty = false;
                frustum.extractFromViewProjection(viewProjMatrix);
            }
        }

        static Vector3 project(const Vector3 &v,
                               const Matrix4x4 &viewProjMatrix,
                               const Viewport &viewport)
        {
            return project(v,
                           viewProjMatrix,
                           static_cast<float>(viewport.width) * 0.5f,
                           static_cast<float>(viewport.height) * 0.5f,
                           viewport.x,
                           viewport.y);
        }
    };
}


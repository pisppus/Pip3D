#pragma once

#include "Body.hpp"

namespace pip3D
{
        __attribute__((always_inline)) inline void RigidBody::setBox(const Vector3 &newSize)
        {
            size = newSize;
            shape = BODY_SHAPE_BOX;
            radius = newSize.x * 0.5f;
            capsuleHalfHeight = 0.0f;
            updateBoundsFromTransform();
            computeInertia();
            updateWorldInvInertia();
        }

        __attribute__((always_inline)) inline void RigidBody::setSphere(float r)
        {
            shape = BODY_SHAPE_SPHERE;
            radius = r;
            size = Vector3(r * 2.0f, r * 2.0f, r * 2.0f);
            capsuleHalfHeight = 0.0f;
            updateBoundsFromTransform();
            computeInertia();
            updateWorldInvInertia();
        }

        __attribute__((always_inline)) inline void RigidBody::setCapsule(float r, float halfHeight)
        {
            shape = BODY_SHAPE_CAPSULE;
            radius = r;
            capsuleHalfHeight = halfHeight;
            size = Vector3(r * 2.0f, 2.0f * (halfHeight + r), r * 2.0f);
            updateBoundsFromTransform();
            computeInertia();
            updateWorldInvInertia();
        }

        inline void RigidBody::setConvex(const Vector3 *verts, int count)
        {
            shape = BODY_SHAPE_CONVEX;
            convexCount = (count > kMaxConvexVerts) ? kMaxConvexVerts : count;
            if (convexCount < 0)
                convexCount = 0;

            Vector3 mn(0, 0, 0), mx(0, 0, 0);
            for (int i = 0; i < convexCount; ++i)
            {
                convexVerts[i] = verts[i];
                if (i == 0)
                {
                    mn = verts[0];
                    mx = verts[0];
                }
                else
                {
                    if (verts[i].x < mn.x)
                        mn.x = verts[i].x;
                    else if (verts[i].x > mx.x)
                        mx.x = verts[i].x;
                    if (verts[i].y < mn.y)
                        mn.y = verts[i].y;
                    else if (verts[i].y > mx.y)
                        mx.y = verts[i].y;
                    if (verts[i].z < mn.z)
                        mn.z = verts[i].z;
                    else if (verts[i].z > mx.z)
                        mx.z = verts[i].z;
                }
            }
            for (int i = convexCount; i < kMaxConvexVerts; ++i)
                convexVerts[i] = Vector3(0, 0, 0);

            size = mx - mn;
            radius = 0.0f;
            for (int i = 0; i < convexCount; ++i)
            {
                float r2 = convexVerts[i].lengthSquared();
                if (r2 > radius * radius)
                    radius = sqrtf(r2);
            }
            capsuleHalfHeight = 0.0f;
            updateBoundsFromTransform();
            computeInertia();
            updateWorldInvInertia();
        }

        __attribute__((always_inline)) inline Vector3
        RigidBody::support(const Vector3 &dirWorld) const noexcept
        {
            Vector3 localDir = orientation.conjugate().rotate(dirWorld);

            if (shape == BODY_SHAPE_SPHERE)
            {
                Vector3 localP;
                float lenSq = localDir.lengthSquared();
                if (lenSq > 1e-12f)
                    localP = localDir * (radius * FastMath::fastInvSqrt(lenSq));
                else
                    localP = Vector3(0, radius, 0);
                return position + orientation.rotate(localP);
            }
            if (shape == BODY_SHAPE_CAPSULE)
            {
                Vector3 localP;
                localP.y = (localDir.y >= 0.0f) ? capsuleHalfHeight : -capsuleHalfHeight;
                float lenSq = localDir.lengthSquared();
                if (lenSq > 1e-12f)
                {
                    float invLen = FastMath::fastInvSqrt(lenSq);
                    localP.x += radius * localDir.x * invLen;
                    localP.y += radius * localDir.y * invLen;
                    localP.z += radius * localDir.z * invLen;
                }
                return position + orientation.rotate(localP);
            }
            if (shape == BODY_SHAPE_CONVEX)
            {
                if (convexCount == 0)
                    return position;
                float bestDot = localDir.dot(convexVerts[0]);
                int bestIdx = 0;
                for (int i = 1; i < convexCount; ++i)
                {
                    float d = localDir.dot(convexVerts[i]);
                    if (d > bestDot)
                    {
                        bestDot = d;
                        bestIdx = i;
                    }
                }
                return position + orientation.rotate(convexVerts[bestIdx]);
            }

            Vector3 half = size * 0.5f;
            Vector3 localP(
                (localDir.x >= 0.0f) ? half.x : -half.x,
                (localDir.y >= 0.0f) ? half.y : -half.y,
                (localDir.z >= 0.0f) ? half.z : -half.z);
            return position + orientation.rotate(localP);
        }

        __attribute__((always_inline)) inline void RigidBody::updateBoundsFromTransform()
        {
            if (shape == BODY_SHAPE_SPHERE)
            {
                bounds = AABB::fromCenterSize(position, size);
                return;
            }

            Vector3 half;
            if (shape == BODY_SHAPE_CAPSULE)
            {
                half.x = radius;
                half.y = capsuleHalfHeight + radius;
                half.z = radius;
            }
            else
            {
                half = size * 0.5f;
            }

            Vector3 ex = orientation.rotate(Vector3(1.0f, 0.0f, 0.0f));
            Vector3 ey = orientation.rotate(Vector3(0.0f, 1.0f, 0.0f));
            Vector3 ez = orientation.rotate(Vector3(0.0f, 0.0f, 1.0f));

            ex.x = fabsf(ex.x);
            ex.y = fabsf(ex.y);
            ex.z = fabsf(ex.z);
            ey.x = fabsf(ey.x);
            ey.y = fabsf(ey.y);
            ey.z = fabsf(ey.z);
            ez.x = fabsf(ez.x);
            ez.y = fabsf(ez.y);
            ez.z = fabsf(ez.z);

            Vector3 r;
            r.x = ex.x * half.x + ey.x * half.y + ez.x * half.z;
            r.y = ex.y * half.x + ey.y * half.y + ez.y * half.z;
            r.z = ex.z * half.x + ey.z * half.y + ez.z * half.z;

            bounds.min = position - r;
            bounds.max = position + r;
        }

}

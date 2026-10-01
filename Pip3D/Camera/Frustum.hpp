#pragma once

#include "Math/Algebra.hpp"

namespace pip3D
{
    struct FrustumPlane
    {
        Vector3 n;
        float d;

        FrustumPlane() : n(0.0f, 1.0f, 0.0f), d(0.0f) {}

        PIP3D_FORCE_INLINE float distanceToPoint(const Vector3 &p) const
        {
            return n.x * p.x + n.y * p.y + n.z * p.z + d;
        }

        PIP3D_FORCE_INLINE bool containsSphere(const Vector3 &center, float radius) const
        {
            return distanceToPoint(center) > -radius;
        }

        PIP3D_FORCE_INLINE bool containsPoint(const Vector3 &p) const
        {
            return distanceToPoint(p) >= 0.0f;
        }
    };

    enum CullingResult
    {
        CULLED = 0,
        PARTIAL = 1,
        VISIBLE = 2
    };

    class CameraFrustum
    {
        FrustumPlane planes[6];

    public:
        enum
        {
            NEAR = 0,
            FAR = 1,
            LEFT = 2,
            RIGHT = 3,
            TOP = 4,
            BOTTOM = 5
        };

        PIP3D_FORCE_INLINE void extractFromViewProjection(const Matrix4x4 &vp)
        {
            const float *PIP3D_RESTRICT m = vp.m;

            planes[NEAR].n.x = m[3] + m[2];
            planes[NEAR].n.y = m[7] + m[6];
            planes[NEAR].n.z = m[11] + m[10];
            planes[NEAR].d = m[15] + m[14];
            planes[FAR].n.x = m[3] - m[2];
            planes[FAR].n.y = m[7] - m[6];
            planes[FAR].n.z = m[11] - m[10];
            planes[FAR].d = m[15] - m[14];
            planes[LEFT].n.x = m[3] + m[0];
            planes[LEFT].n.y = m[7] + m[4];
            planes[LEFT].n.z = m[11] + m[8];
            planes[LEFT].d = m[15] + m[12];
            planes[RIGHT].n.x = m[3] - m[0];
            planes[RIGHT].n.y = m[7] - m[4];
            planes[RIGHT].n.z = m[11] - m[8];
            planes[RIGHT].d = m[15] - m[12];
            planes[TOP].n.x = m[3] - m[1];
            planes[TOP].n.y = m[7] - m[5];
            planes[TOP].n.z = m[11] - m[9];
            planes[TOP].d = m[15] - m[13];
            planes[BOTTOM].n.x = m[3] + m[1];
            planes[BOTTOM].n.y = m[7] + m[5];
            planes[BOTTOM].n.z = m[11] + m[9];
            planes[BOTTOM].d = m[15] + m[13];

            for (int i = 0; i < 6; ++i)
            {
                const float lenSq = planes[i].n.lengthSquared();
                const float invLen = FastMath::fastInvSqrt(lenSq);
                planes[i].n *= invLen;
                planes[i].d *= invLen;
            }
        }

        PIP3D_FORCE_INLINE bool testSphere(const Vector3 &center, float radius) const
        {
            if (unlikely(planes[NEAR].distanceToPoint(center) < -radius))
                return false;
            if (unlikely(planes[FAR].distanceToPoint(center) < -radius))
                return false;
            if (unlikely(planes[LEFT].distanceToPoint(center) < -radius))
                return false;
            if (unlikely(planes[RIGHT].distanceToPoint(center) < -radius))
                return false;
            if (unlikely(planes[TOP].distanceToPoint(center) < -radius))
                return false;
            if (unlikely(planes[BOTTOM].distanceToPoint(center) < -radius))
                return false;
            return true;
        }

        PIP3D_FORCE_INLINE bool testAABB(const Vector3 &min, const Vector3 &max) const
        {
            for (int i = 0; i < 6; ++i)
            {
                const Vector3 p(
                    planes[i].n.x > 0.0f ? max.x : min.x,
                    planes[i].n.y > 0.0f ? max.y : min.y,
                    planes[i].n.z > 0.0f ? max.z : min.z);
                if (unlikely(planes[i].distanceToPoint(p) < 0.0f))
                    return false;
            }
            return true;
        }

        PIP3D_FORCE_INLINE bool testPoint(const Vector3 &p) const
        {
            return planes[NEAR].distanceToPoint(p) >= 0.0f &&
                   planes[FAR].distanceToPoint(p) >= 0.0f &&
                   planes[LEFT].distanceToPoint(p) >= 0.0f &&
                   planes[RIGHT].distanceToPoint(p) >= 0.0f &&
                   planes[TOP].distanceToPoint(p) >= 0.0f &&
                   planes[BOTTOM].distanceToPoint(p) >= 0.0f;
        }

        PIP3D_FORCE_INLINE const FrustumPlane &getPlane(int i) const { return planes[i]; }
    };

    using Frustum = CameraFrustum;
}
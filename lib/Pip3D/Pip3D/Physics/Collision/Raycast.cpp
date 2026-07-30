#include "Physics/World.hpp"
#include "Physics/Collision/Raycast.hpp"

#include <cfloat>
#include <cmath>

#include "Math/Collision.hpp"
#include "Physics/RigidBody/Body.hpp"

namespace pip3D
{

    namespace
    {

        PIP3D_NOINLINE bool
        raycastBoxLike(const Ray &ray,
                       const RigidBody *b,
                       float maxDistance,
                       float &outT,
                       Vector3 &outNormal) noexcept
        {
            const Vector3 half = b->size * 0.5f;
            const Quaternion invRot = b->orientation.conjugate();
            const Vector3 localOrigin = invRot.rotate(ray.origin - b->position);
            const Vector3 localDir = invRot.rotate(ray.direction);
            const Ray localRay(localOrigin, localDir);

            const AABB localBox(Vector3(-half.x, -half.y, -half.z),
                                Vector3(half.x, half.y, half.z));

            float tMin, tMax;
            if (!localRay.intersects(localBox, tMin, tMax))
                return false;

            const float tHit = (tMin >= 0.0f) ? tMin : tMax;
            if (tHit < 0.0f || tHit > maxDistance)
                return false;

            outT = tHit;

            const Vector3 localHit = localRay.at(tHit);
            const float dx = half.x - fabsf(localHit.x);
            const float dy = half.y - fabsf(localHit.y);
            const float dz = half.z - fabsf(localHit.z);

            Vector3 localN(0.0f, 1.0f, 0.0f);
            if (dx <= dy && dx <= dz)
                localN = Vector3((localHit.x > 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);
            else if (dy <= dz)
                localN = Vector3(0.0f, (localHit.y > 0.0f) ? 1.0f : -1.0f, 0.0f);
            else
                localN = Vector3(0.0f, 0.0f, (localHit.z > 0.0f) ? 1.0f : -1.0f);

            outNormal = b->orientation.rotate(localN);
            return true;
        }

        PIP3D_NOINLINE bool
        raycastSphere(const Ray &ray,
                      const RigidBody *b,
                      float maxDistance,
                      float &outT,
                      Vector3 &outNormal) noexcept
        {
            const CollisionSphere s(b->position, b->radius);
            float t;
            if (!ray.intersects(s, t) || t < 0.0f || t > maxDistance)
                return false;

            outT = t;
            const Vector3 hitPoint = ray.at(t);
            Vector3 n = hitPoint - b->position;
            const float nLenSq = n.lengthSquared();
            if (nLenSq > 1e-8f)
                n *= FastMath::fastInvSqrt(nLenSq);
            else
                n = Vector3(0.0f, 1.0f, 0.0f);
            outNormal = n;
            return true;
        }

        PIP3D_NOINLINE bool
        raycastCapsule(const Ray &ray,
                       const RigidBody *b,
                       float maxDistance,
                       float &outT,
                       Vector3 &outNormal) noexcept
        {
            const Vector3 axisY = b->orientation.rotate(
                Vector3(0.0f, b->capsuleHalfHeight, 0.0f));
            const Vector3 p0 = b->position - axisY;
            const Vector3 p1 = b->position + axisY;

            const CollisionSphere s0(p0, b->radius);
            const CollisionSphere s1(p1, b->radius);

            float t0, t1;
            const bool h0 = ray.intersects(s0, t0) && t0 >= 0.0f && t0 <= maxDistance;
            const bool h1 = ray.intersects(s1, t1) && t1 >= 0.0f && t1 <= maxDistance;

            float tHit = FLT_MAX;
            Vector3 centerHit = p0;
            if (h0 && t0 < tHit)
            {
                tHit = t0;
                centerHit = p0;
            }
            if (h1 && t1 < tHit)
            {
                tHit = t1;
                centerHit = p1;
            }
            if (tHit >= FLT_MAX)
                return false;

            outT = tHit;
            const Vector3 hitPt = ray.at(tHit);
            Vector3 n = hitPt - centerHit;
            const float nLenSq = n.lengthSquared();
            if (nLenSq > 1e-8f)
                n *= FastMath::fastInvSqrt(nLenSq);
            else
                n = Vector3(0.0f, 1.0f, 0.0f);
            outNormal = n;
            return true;
        }
    }

    bool PhysicsWorld::raycast(const Ray &ray, RaycastHit &outHit, float maxDistance)
    {

        outHit.hit = false;
        outHit.body = nullptr;
        outHit.distance = maxDistance;

        const int bodyCount = bodyCount_;
        for (int i = 0; i < bodyCount; ++i)
        {
            const RigidBody *b = bodies_[i];
            if (!b)
                continue;

            float tMinAABB, tMaxAABB;
            if (!ray.intersects(b->bounds, tMinAABB, tMaxAABB))
                continue;
            if (tMaxAABB < 0.0f)
                continue;

            float bestT = FLT_MAX;
            Vector3 hitNormal(0.0f, 1.0f, 0.0f);
            bool hitFound = false;

            switch (b->shape)
            {
            case BODY_SHAPE_SPHERE:
                hitFound = raycastSphere(ray, b, maxDistance, bestT, hitNormal);
                break;

            case BODY_SHAPE_CAPSULE:
            case BODY_SHAPE_CYLINDER:
                hitFound = raycastCapsule(ray, b, maxDistance, bestT, hitNormal);
                break;

            case BODY_SHAPE_CONVEX:
            case BODY_SHAPE_BOX:
            default:
                hitFound = raycastBoxLike(ray, b, maxDistance, bestT, hitNormal);
                break;
            }

            if (!hitFound)
                continue;

            if (bestT < outHit.distance)
            {
                outHit.hit = true;
                outHit.body = const_cast<RigidBody *>(b);
                outHit.distance = bestT;
                outHit.point = ray.at(bestT);
                outHit.normal = hitNormal;
            }
        }

        return outHit.hit;
    }

}

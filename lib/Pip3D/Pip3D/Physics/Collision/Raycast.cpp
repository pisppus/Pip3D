#include "../World.hpp"
#include "Raycast.hpp"

#include <cfloat>

namespace pip3D
{
    bool PhysicsWorld::raycast(const Ray &ray, RaycastHit &outHit, float maxDistance)
    {
        outHit.hit = false;
        outHit.body = nullptr;
        outHit.distance = maxDistance;

        const size_t bodyCount = bodies.size();
        for (size_t i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies[i];
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

            if (b->shape == BODY_SHAPE_SPHERE)
            {
                CollisionSphere s(b->position, b->radius);
                float tSphere;
                if (ray.intersects(s, tSphere) && tSphere >= 0.0f && tSphere <= maxDistance)
                {
                    bestT = tSphere;
                    Vector3 hitPoint = ray.at(tSphere);
                    Vector3 n = hitPoint - b->position;
                    float nLenSq = n.lengthSquared();
                    if (nLenSq > 1e-8f)
                        n *= FastMath::fastInvSqrt(nLenSq);
                    else
                        n = Vector3(0.0f, 1.0f, 0.0f);
                    hitNormal = n;
                    hitFound = true;
                }
            }
            else if (b->shape == BODY_SHAPE_CAPSULE)
            {

                Vector3 p0, p1;
                {
                    Vector3 axisY = b->orientation.rotate(
                        Vector3(0.0f, b->capsuleHalfHeight, 0.0f));
                    p0 = b->position - axisY;
                    p1 = b->position + axisY;
                }
                CollisionSphere s0(p0, b->radius);
                CollisionSphere s1(p1, b->radius);
                float t0, t1;
                bool h0 = ray.intersects(s0, t0) && t0 >= 0.0f && t0 <= maxDistance;
                bool h1 = ray.intersects(s1, t1) && t1 >= 0.0f && t1 <= maxDistance;
                float tHit = FLT_MAX;
                Vector3 hitPt(0, 0, 0);
                Vector3 centerHit = p0;
                if (h0 && t0 < tHit)
                {
                    tHit = t0;
                    hitPt = ray.at(t0);
                    centerHit = p0;
                }
                if (h1 && t1 < tHit)
                {
                    tHit = t1;
                    hitPt = ray.at(t1);
                    centerHit = p1;
                }
                if (tHit < FLT_MAX)
                {
                    Vector3 n = hitPt - centerHit;
                    float nLenSq = n.lengthSquared();
                    if (nLenSq > 1e-8f)
                        n *= FastMath::fastInvSqrt(nLenSq);
                    else
                        n = Vector3(0.0f, 1.0f, 0.0f);
                    bestT = tHit;
                    hitNormal = n;
                    hitFound = true;
                }
            }
            else if (b->shape == BODY_SHAPE_CONVEX)
            {

                Vector3 half = b->size * 0.5f;
                Quaternion invRot = b->orientation.conjugate();
                Vector3 localOrigin = invRot.rotate(ray.origin - b->position);
                Vector3 localDir = invRot.rotate(ray.direction);
                Ray localRay(localOrigin, localDir);
                AABB localBox(Vector3(-half.x, -half.y, -half.z),
                              Vector3(half.x, half.y, half.z));
                float tMin, tMax;
                if (localRay.intersects(localBox, tMin, tMax))
                {
                    float tHit = tMin >= 0.0f ? tMin : tMax;
                    if (tHit >= 0.0f && tHit <= maxDistance)
                    {
                        bestT = tHit;
                        Vector3 localHit = localRay.at(tHit);
                        float dx = half.x - fabsf(localHit.x);
                        float dy = half.y - fabsf(localHit.y);
                        float dz = half.z - fabsf(localHit.z);
                        Vector3 localN(0.0f, 1.0f, 0.0f);
                        if (dx <= dy && dx <= dz)
                            localN = Vector3((localHit.x > 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);
                        else if (dy <= dz)
                            localN = Vector3(0.0f, (localHit.y > 0.0f) ? 1.0f : -1.0f, 0.0f);
                        else
                            localN = Vector3(0.0f, 0.0f, (localHit.z > 0.0f) ? 1.0f : -1.0f);
                        hitNormal = b->orientation.rotate(localN);
                        hitFound = true;
                    }
                }
            }
            else
            {
                Vector3 half = b->size * 0.5f;
                Quaternion invRot = b->orientation.conjugate();
                Vector3 localOrigin = invRot.rotate(ray.origin - b->position);
                Vector3 localDir = invRot.rotate(ray.direction);
                Ray localRay(localOrigin, localDir);
                AABB localBox(Vector3(-half.x, -half.y, -half.z),
                              Vector3(half.x, half.y, half.z));
                float tMin, tMax;
                if (localRay.intersects(localBox, tMin, tMax))
                {
                    float tHit = tMin >= 0.0f ? tMin : tMax;
                    if (tHit >= 0.0f && tHit <= maxDistance)
                    {
                        bestT = tHit;
                        Vector3 localHit = localRay.at(tHit);
                        float dx = half.x - fabsf(localHit.x);
                        float dy = half.y - fabsf(localHit.y);
                        float dz = half.z - fabsf(localHit.z);
                        Vector3 localN(0.0f, 1.0f, 0.0f);
                        if (dx <= dy && dx <= dz)
                            localN = Vector3((localHit.x > 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);
                        else if (dy <= dz)
                            localN = Vector3(0.0f, (localHit.y > 0.0f) ? 1.0f : -1.0f, 0.0f);
                        else
                            localN = Vector3(0.0f, 0.0f, (localHit.z > 0.0f) ? 1.0f : -1.0f);
                        hitNormal = b->orientation.rotate(localN);
                        hitFound = true;
                    }
                }
            }

            if (!hitFound)
                continue;

            if (bestT < outHit.distance)
            {
                outHit.hit = true;
                outHit.body = b;
                outHit.distance = bestT;
                outHit.point = ray.at(bestT);
                outHit.normal = hitNormal;
            }
        }

        return outHit.hit;
    }
}

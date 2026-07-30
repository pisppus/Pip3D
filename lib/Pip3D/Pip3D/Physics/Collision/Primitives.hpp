#pragma once

#include "Helpers.hpp"

namespace pip3D
{

#define PIP3D_INIT_MANIFOLD(info, aPtr, bPtr)                       \
    do                                                              \
    {                                                               \
        (info).hasCollision = false;                                \
        (info).hasRealContact = false;                              \
        (info).bodyA = (aPtr);                                      \
        (info).bodyB = (bPtr);                                      \
        (info).contactCount = 0;                                    \
        (info).normal = Vector3(0, 1, 0);                           \
        for (int i = 0; i < PhysicsConfig::MAX_CONTACT_POINTS; ++i) \
        {                                                           \
            (info).contacts[i].pos = Vector3(0, 0, 0);              \
            (info).contacts[i].localPointA = Vector3(0, 0, 0);      \
            (info).contacts[i].localPointB = Vector3(0, 0, 0);      \
            (info).contacts[i].penetration = 0.0f;                  \
            (info).contacts[i].accumulatedImpulse = 0.0f;           \
            (info).contacts[i].tangentImpulse1 = 0.0f;              \
            (info).contacts[i].tangentImpulse2 = 0.0f;              \
            (info).contacts[i].normalMass = 0.0f;                   \
            (info).contacts[i].tangentMass1 = 0.0f;                 \
            (info).contacts[i].tangentMass2 = 0.0f;                 \
            (info).contacts[i].bias = 0.0f;                         \
            (info).contacts[i].material = 0;                        \
            (info).contacts[i].lifetime = 0;                        \
            (info).contacts[i].featureId = 0;                       \
        }                                                           \
    } while (0)

    inline ContactManifold detectSphereSphere(RigidBody *a, RigidBody *b, float)
    {
        ContactManifold info;
        PIP3D_INIT_MANIFOLD(info, a, b);

        Vector3 centerA = a->position;
        Vector3 centerB = b->position;
        Vector3 delta = centerB - centerA;
        float distSq = delta.lengthSquared();
        float radiusSum = a->radius + b->radius;

        if (distSq <= radiusSum * radiusSum)
        {
            float dist = distSq > 1e-8f ? sqrtf(distSq) : 0.0f;
            Vector3 normal;
            if (dist > 1e-4f)
                normal = delta * FastMath::fastInvSqrt(distSq);
            else
                normal = Vector3(0, 1, 0);
            float penetration = radiusSum - dist;
            Vector3 contact = centerA + normal * (a->radius - penetration * 0.5f);

            info.hasCollision = true;
            info.bodyA = a;
            info.bodyB = b;
            info.normal = normal;
            info.contactCount = 1;
            info.contacts[0].pos = contact;
            info.contacts[0].penetration = penetration;
            info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, 0u);
            return info;
        }

        return info;
    }

    inline ContactManifold detectSphereBox(RigidBody *sphere, RigidBody *box, float)
    {
        ContactManifold info;
        PIP3D_INIT_MANIFOLD(info, sphere, box);

        Vector3 sphereCenter = sphere->position;
        Vector3 boxCenter = box->position;
        Vector3 halfExtents = box->size * 0.5f;

        Quaternion invRot = box->orientation.conjugate();
        Vector3 local = invRot.rotate(sphereCenter - boxCenter);

        float lx = fmaxf(-halfExtents.x, fminf(local.x, halfExtents.x));
        float ly = fmaxf(-halfExtents.y, fminf(local.y, halfExtents.y));
        float lz = fmaxf(-halfExtents.z, fminf(local.z, halfExtents.z));
        Vector3 closestLocal(lx, ly, lz);
        Vector3 closestWorld = box->orientation.rotate(closestLocal) + boxCenter;

        Vector3 diff = closestWorld - sphereCenter;
        float distSq = diff.lengthSquared();
        float r = sphere->radius;

        if (distSq <= r * r)
        {
            float dist = distSq > 1e-8f ? sqrtf(distSq) : 0.0f;
            Vector3 normal;
            float penetration;
            if (dist > 1e-4f)
            {
                normal = diff * FastMath::fastInvSqrt(distSq);
                penetration = r - dist;
            }
            else
            {
                float dx = halfExtents.x - fabsf(local.x);
                float dy = halfExtents.y - fabsf(local.y);
                float dz = halfExtents.z - fabsf(local.z);
                Vector3 localN(0, -1, 0);
                if (dx < dy && dx < dz)
                    localN = Vector3((local.x > 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);
                else if (dy < dz)
                    localN = Vector3(0.0f, (local.y > 0.0f) ? 1.0f : -1.0f, 0.0f);
                else
                    localN = Vector3(0.0f, 0.0f, (local.z > 0.0f) ? 1.0f : -1.0f);
                normal = box->orientation.rotate(localN);
                penetration = r;
            }

            info.hasCollision = true;
            info.bodyA = sphere;
            info.bodyB = box;
            info.normal = normal;
            info.contactCount = 1;
            info.contacts[0].pos = closestWorld;
            info.contacts[0].penetration = penetration;
            info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, 0u);
            return info;
        }

        return info;
    }

    inline ContactManifold detectBoxBox(RigidBody *a, RigidBody *b)
    {
        ContactManifold info;
        PIP3D_INIT_MANIFOLD(info, a, b);

        const float eps = 1e-4f;
        Vector3 Ca = a->position;
        Vector3 Cb = b->position;

        Vector3 Aa[3] = {
            a->orientation.rotate(Vector3(1, 0, 0)),
            a->orientation.rotate(Vector3(0, 1, 0)),
            a->orientation.rotate(Vector3(0, 0, 1))};
        Vector3 Ab[3] = {
            b->orientation.rotate(Vector3(1, 0, 0)),
            b->orientation.rotate(Vector3(0, 1, 0)),
            b->orientation.rotate(Vector3(0, 0, 1))};

        Vector3 Ea = a->size * 0.5f;
        Vector3 Eb = b->size * 0.5f;

        float R[3][3];
        float AbsR[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
            {
                float v = Aa[i].dot(Ab[j]);
                R[i][j] = v;
                AbsR[i][j] = fabsf(v) + eps;
            }

        Vector3 tWorld = Cb - Ca;
        float t[3] = {tWorld.dot(Aa[0]), tWorld.dot(Aa[1]), tWorld.dot(Aa[2])};

        float minPenetration = FLT_MAX;
        Vector3 bestAxis(0, 1, 0);
        int bestType = 0;
        int bestAxisIdx = 0;
        int bestEdgeJ = 0;

        for (int i = 0; i < 3; ++i)
        {
            float ra = (i == 0 ? Ea.x : (i == 1 ? Ea.y : Ea.z));
            float rb = Eb.x * AbsR[i][0] + Eb.y * AbsR[i][1] + Eb.z * AbsR[i][2];
            float dist = fabsf(t[i]);
            float pen = ra + rb - dist;
            if (pen < 0.0f)
                return info;
            if (pen < minPenetration)
            {
                minPenetration = pen;
                bestAxis = Aa[i] * (t[i] < 0.0f ? -1.0f : 1.0f);
                bestType = 0;
                bestAxisIdx = i;
            }
        }

        for (int j = 0; j < 3; ++j)
        {
            float ra = Ea.x * AbsR[0][j] + Ea.y * AbsR[1][j] + Ea.z * AbsR[2][j];
            float rb = (j == 0 ? Eb.x : (j == 1 ? Eb.y : Eb.z));
            float dist = fabsf(Cb.dot(Ab[j]) - Ca.dot(Ab[j]));
            float pen = ra + rb - dist;
            if (pen < 0.0f)
                return info;
            if (pen < minPenetration)
            {
                minPenetration = pen;
                float sign = (tWorld.dot(Ab[j]) < 0.0f) ? -1.0f : 1.0f;
                bestAxis = Ab[j] * sign;
                bestType = 1;
                bestAxisIdx = j;
            }
        }

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                Vector3 axis = Aa[i].cross(Ab[j]);
                float axisLenSq = axis.lengthSquared();
                if (axisLenSq < 1e-8f)
                    continue;
                float invLen = FastMath::fastInvSqrt(axisLenSq);
                axis *= invLen;

                float ra = Ea.x * fabsf(axis.dot(Aa[0])) + Ea.y * fabsf(axis.dot(Aa[1])) + Ea.z * fabsf(axis.dot(Aa[2]));
                float rb = Eb.x * fabsf(axis.dot(Ab[0])) + Eb.y * fabsf(axis.dot(Ab[1])) + Eb.z * fabsf(axis.dot(Ab[2]));
                float dist = fabsf(axis.dot(tWorld));
                float pen = ra + rb - dist;
                if (pen < 0.0f)
                    return info;

                if (pen < minPenetration - 1e-3f)
                {
                    minPenetration = pen;
                    bestAxis = axis * (axis.dot(tWorld) < 0.0f ? -1.0f : 1.0f);
                    bestType = 2;
                    bestAxisIdx = i;
                    bestEdgeJ = j;
                }
            }
        }

        if (minPenetration <= 0.0f)
            return info;

        Vector3 n = bestAxis;

        if (bestType == 2)
        {
            int i = bestAxisIdx;
            int j = bestEdgeJ;

            int ai1 = (i + 1) % 3;
            int ai2 = (i + 2) % 3;
            float Ea_i = (i == 0) ? Ea.x : (i == 1) ? Ea.y
                                                    : Ea.z;
            float Ea_1 = (ai1 == 0) ? Ea.x : (ai1 == 1) ? Ea.y
                                                        : Ea.z;
            float Ea_2 = (ai2 == 0) ? Ea.x : (ai2 == 1) ? Ea.y
                                                        : Ea.z;

            int bj1 = (j + 1) % 3;
            int bj2 = (j + 2) % 3;
            float Eb_j = (j == 0) ? Eb.x : (j == 1) ? Eb.y
                                                    : Eb.z;
            float Eb_1 = (bj1 == 0) ? Eb.x : (bj1 == 1) ? Eb.y
                                                        : Eb.z;
            float Eb_2 = (bj2 == 0) ? Eb.x : (bj2 == 1) ? Eb.y
                                                        : Eb.z;

            float sA1 = (tWorld.dot(Aa[ai1]) < 0.0f) ? -1.0f : 1.0f;
            float sA2 = (tWorld.dot(Aa[ai2]) < 0.0f) ? -1.0f : 1.0f;
            Vector3 edgeACenter = Ca + Aa[ai1] * (sA1 * Ea_1) + Aa[ai2] * (sA2 * Ea_2);
            Vector3 edgeA0 = edgeACenter - Aa[i] * Ea_i;
            Vector3 edgeA1 = edgeACenter + Aa[i] * Ea_i;

            Vector3 negT = Ca - Cb;
            float sB1 = (negT.dot(Ab[bj1]) < 0.0f) ? -1.0f : 1.0f;
            float sB2 = (negT.dot(Ab[bj2]) < 0.0f) ? -1.0f : 1.0f;
            Vector3 edgeBCenter = Cb + Ab[bj1] * (sB1 * Eb_1) + Ab[bj2] * (sB2 * Eb_2);
            Vector3 edgeB0 = edgeBCenter - Ab[j] * Eb_j;
            Vector3 edgeB1 = edgeBCenter + Ab[j] * Eb_j;

            float s, tParam;
            Vector3 c1, c2;
            closestPtSegmentSegment(edgeA0, edgeA1, edgeB0, edgeB1, s, tParam, c1, c2);

            info.hasCollision = true;
            info.bodyA = a;
            info.bodyB = b;
            info.normal = n;
            info.contactCount = 1;
            info.contacts[0].pos = (c1 + c2) * 0.5f;
            info.contacts[0].penetration = minPenetration;
            info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_EDGE,
                                                       (static_cast<uint32_t>(i) << 4) | static_cast<uint32_t>(j));
            return info;
        }

        int refIndex = bestAxisIdx;
        bool refOnB = (bestType == 1);

        Vector3 refNormal = refOnB ? Ab[refIndex] : Aa[refIndex];
        float refExtent = refOnB
                              ? ((refIndex == 0) ? Eb.x : (refIndex == 1) ? Eb.y
                                                                          : Eb.z)
                              : ((refIndex == 0) ? Ea.x : (refIndex == 1) ? Ea.y
                                                                          : Ea.z);
        float refSign;
        Vector3 refCenter;
        if (!refOnB)
        {
            refSign = (n.dot(Aa[refIndex]) >= 0.0f) ? 1.0f : -1.0f;
            refCenter = Ca + Aa[refIndex] * (refSign * refExtent);
        }
        else
        {
            refSign = (n.dot(Ab[refIndex]) >= 0.0f) ? -1.0f : 1.0f;
            refCenter = Cb + Ab[refIndex] * (refSign * refExtent);
        }
        float planeD = n.dot(refCenter);

        Vector3 *incAxes = refOnB ? Aa : Ab;
        Vector3 incCenter = refOnB ? Ca : Cb;
        Vector3 incHalf = refOnB ? Ea : Eb;

        int incIndex = 0;
        float maxAbsDot = fabsf(incAxes[0].dot(n));
        for (int j = 1; j < 3; ++j)
        {
            float ad = fabsf(incAxes[j].dot(n));
            if (ad > maxAbsDot)
            {
                maxAbsDot = ad;
                incIndex = j;
            }
        }

        Vector3 incidentLocal[4];
        Vector3 incAxisN = incAxes[incIndex];
        float incSign = (incAxisN.dot(n) < 0.0f) ? 1.0f : -1.0f;
        float incExt = (incIndex == 0) ? incHalf.x : (incIndex == 1) ? incHalf.y
                                                                     : incHalf.z;

        int oi1 = (incIndex + 1) % 3;
        int oi2 = (incIndex + 2) % 3;
        float oe1 = (oi1 == 0) ? incHalf.x : (oi1 == 1) ? incHalf.y
                                                        : incHalf.z;
        float oe2 = (oi2 == 0) ? incHalf.x : (oi2 == 1) ? incHalf.y
                                                        : incHalf.z;
        Vector3 oa1 = incAxes[oi1];
        Vector3 oa2 = incAxes[oi2];
        Vector3 faceBase = incAxisN * (incSign * incExt);

        incidentLocal[0] = faceBase + oa1 * (-oe1) + oa2 * (-oe2);
        incidentLocal[1] = faceBase + oa1 * (oe1) + oa2 * (-oe2);
        incidentLocal[2] = faceBase + oa1 * (oe1) + oa2 * (oe2);
        incidentLocal[3] = faceBase + oa1 * (-oe1) + oa2 * (oe2);

        Vector3 incidentWorld[4];
        for (int i = 0; i < 4; ++i)
            incidentWorld[i] = incidentLocal[i] + incCenter;

        info.hasCollision = true;
        info.bodyA = a;
        info.bodyB = b;
        info.normal = n;
        info.contactCount = 0;

        const uint32_t faceKeyBase = static_cast<uint32_t>(incIndex) << 3;
        const float contactEps = PhysicsConfig::MANIFOLD_CONTACT_EPS;

        for (int i = 0; i < 4; ++i)
        {
            Vector3 p = incidentWorld[i];
            float dist = n.dot(p) - planeD;
            if (dist <= contactEps && info.contactCount < PhysicsConfig::MAX_CONTACT_POINTS)
            {
                Contact &c = info.contacts[info.contactCount++];
                c.pos = p - n * (dist > 0.0f ? 0.0f : dist);
                float pen = -dist;
                if (pen < 0.0f)
                    pen = 0.0f;
                if (pen > minPenetration)
                    pen = minPenetration;
                c.penetration = pen;
                c.featureId = makeFeatureId(CONTACT_FEATURE_FACE,
                                            faceKeyBase | static_cast<uint32_t>(i));
            }
        }

        if (info.contactCount == 0)
        {
            info.contactCount = 1;
            info.contacts[0].pos = (Ca + Cb) * 0.5f;
            info.contacts[0].penetration = minPenetration;
            info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_FACE, faceKeyBase);
        }

        return info;
    }

    inline ContactManifold detectCapsuleSphere(RigidBody *cap, RigidBody *sphere)
    {
        ContactManifold info;
        PIP3D_INIT_MANIFOLD(info, cap, sphere);

        Vector3 p0, p1;
        capsuleEndpoints(cap, p0, p1);

        Vector3 sphereCenter = sphere->position;
        float tParam;
        Vector3 closest = closestPtOnSegment(sphereCenter, p0, p1, tParam);

        Vector3 diff = sphereCenter - closest;
        float distSq = diff.lengthSquared();
        float rSum = cap->radius + sphere->radius;

        if (distSq > rSum * rSum)
            return info;

        float dist = distSq > 1e-8f ? sqrtf(distSq) : 0.0f;
        Vector3 normal;
        float penetration;
        if (dist > 1e-4f)
        {
            normal = diff * FastMath::fastInvSqrt(distSq);
            penetration = rSum - dist;
        }
        else
        {
            Vector3 seg = p1 - p0;
            float segLenSq = seg.lengthSquared();
            if (segLenSq > 1e-8f)
                seg *= FastMath::fastInvSqrt(segLenSq);
            else
                seg = Vector3(0, 1, 0);
            normal = seg;
            penetration = rSum;
        }

        info.hasCollision = true;
        info.bodyA = cap;
        info.bodyB = sphere;
        info.normal = normal;
        info.contactCount = 1;
        info.contacts[0].pos = closest + normal * cap->radius;
        info.contacts[0].penetration = penetration;
        info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, 0u);
        return info;
    }

    inline ContactManifold detectCapsuleBox(RigidBody *cap, RigidBody *box)
    {
        ContactManifold info;
        PIP3D_INIT_MANIFOLD(info, cap, box);

        Vector3 p0, p1;
        capsuleEndpoints(cap, p0, p1);

        Vector3 boxCenter = box->position;
        Vector3 halfExtents = box->size * 0.5f;
        Quaternion invRot = box->orientation.conjugate();

        Vector3 aLocal = invRot.rotate(p0 - boxCenter);
        Vector3 bLocal = invRot.rotate(p1 - boxCenter);
        Vector3 mn(-halfExtents.x, -halfExtents.y, -halfExtents.z);
        Vector3 mx(halfExtents.x, halfExtents.y, halfExtents.z);

        Vector3 segPt, boxPt;
        float distSq = closestSegAABB(aLocal, bLocal, mn, mx, segPt, boxPt);
        float r = cap->radius;
        float rSq = r * r;

        if (distSq > rSq)
            return info;

        Vector3 localNormal;
        float penetration;
        if (distSq > 1e-8f)
        {
            float dist = sqrtf(distSq);
            localNormal = (segPt - boxPt) * FastMath::fastInvSqrt(distSq);
            penetration = r - dist;
        }
        else
        {
            float dx = halfExtents.x - fabsf(segPt.x);
            float dy = halfExtents.y - fabsf(segPt.y);
            float dz = halfExtents.z - fabsf(segPt.z);
            Vector3 localN(0, -1, 0);
            if (dx < dy && dx < dz)
                localN = Vector3((segPt.x > 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);
            else if (dy < dz)
                localN = Vector3(0.0f, (segPt.y > 0.0f) ? 1.0f : -1.0f, 0.0f);
            else
                localN = Vector3(0.0f, 0.0f, (segPt.z > 0.0f) ? 1.0f : -1.0f);
            localNormal = localN;
            penetration = r + fminf(fminf(dx, dy), dz);
        }

        Vector3 worldNormal = box->orientation.rotate(localNormal);
        Vector3 nAB = worldNormal * -1.0f;
        Vector3 boxPtWorld = box->orientation.rotate(boxPt) + boxCenter;

        info.hasCollision = true;
        info.bodyA = cap;
        info.bodyB = box;
        info.normal = nAB;
        info.contactCount = 0;

        {
            Contact &c = info.contacts[info.contactCount++];
            c.pos = boxPtWorld;
            c.penetration = penetration;
            c.featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, 0u);
        }

        const float kDedupDistSq = 0.005f * 0.005f;
        const Vector3 endpoints[2] = {p0, p1};
        const uint32_t endpointKeys[2] = {1u, 2u};

        for (int ei = 0; ei < 2; ++ei)
        {
            if (info.contactCount >= PhysicsConfig::MAX_CONTACT_POINTS)
                break;

            Vector3 epNormal;
            float epPen;
            Vector3 epContactWorld;
            if (!spherePointContactVsBox(endpoints[ei], r, box, epNormal, epPen, epContactWorld))
                continue;
            if (epPen <= 0.0f)
                continue;

            bool dup = false;
            for (int j = 0; j < info.contactCount; ++j)
            {
                Vector3 d = info.contacts[j].pos - epContactWorld;
                if (d.lengthSquared() < kDedupDistSq)
                {
                    dup = true;
                    break;
                }
            }
            if (dup)
                continue;

            Contact &c = info.contacts[info.contactCount++];
            c.pos = epContactWorld;
            c.penetration = epPen;
            c.featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, endpointKeys[ei]);
        }

        return info;
    }

    inline ContactManifold detectCapsuleCapsule(RigidBody *a, RigidBody *b)
    {
        ContactManifold info;
        PIP3D_INIT_MANIFOLD(info, a, b);

        Vector3 a0, a1, b0, b1;
        capsuleEndpoints(a, a0, a1);
        capsuleEndpoints(b, b0, b1);

        float s, t;
        Vector3 c1, c2;
        closestPtSegmentSegment(a0, a1, b0, b1, s, t, c1, c2);

        Vector3 diff = c2 - c1;
        float distSq = diff.lengthSquared();
        float rSum = a->radius + b->radius;

        if (distSq > rSum * rSum)
            return info;

        float dist = distSq > 1e-8f ? sqrtf(distSq) : 0.0f;
        Vector3 normal;
        float penetration;
        if (dist > 1e-4f)
        {
            normal = diff * FastMath::fastInvSqrt(distSq);
            penetration = rSum - dist;
        }
        else
        {
            normal = Vector3(0, 1, 0);
            penetration = rSum;
        }

        info.hasCollision = true;
        info.bodyA = a;
        info.bodyB = b;
        info.normal = normal;
        info.contactCount = 1;
        info.contacts[0].pos = (c1 + c2) * 0.5f;
        info.contacts[0].penetration = penetration;
        info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, 0u);
        return info;
    }
}

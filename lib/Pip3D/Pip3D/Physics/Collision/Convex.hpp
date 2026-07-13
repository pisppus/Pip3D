#pragma once

#include "Helpers.hpp"
#include "GJK.hpp"
#include "Simplex.hpp"
#include "EPA.hpp"

namespace pip3D
{
    inline ContactManifold detectConvexConvex(RigidBody *a, RigidBody *b)
    {
        ContactManifold info;
        if (!a || !b)
            return info;

        GJKVertex simplex[4];
        int simplexSize = 0;
        if (!gjkIntersect(a, b, simplex, simplexSize))
            return info;

        EPAResult epa;
        if (!epaPenetration(a, b, simplex, simplexSize, epa))
            return info;

        info.hasCollision = true;
        info.bodyA = a;
        info.bodyB = b;
        info.normal = epa.normal;
        info.contactCount = 1;
        info.contacts[0].pos = (epa.contactPointA + epa.contactPointB) * 0.5f;
        info.contacts[0].penetration = epa.depth;
        info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, 0u);

        Vector3 n = epa.normal;
        Vector3 t1, t2;

        if (fabsf(n.x) <= 0.70710678f)
            t1 = Vector3(1.0f - n.x * n.x, -n.y * n.x, -n.z * n.x);
        else
            t1 = Vector3(-n.x * n.y, 1.0f - n.y * n.y, -n.z * n.y);
        t1.normalize();
        t2 = n.cross(t1);

        float radiusA = 0.0f;
        for (int i = 0; i < a->convexCount; ++i)
        {
            float r2 = a->convexVerts[i].lengthSquared();
            if (r2 > radiusA * radiusA)
                radiusA = r2;
        }
        radiusA = sqrtf(radiusA);
        float radiusB = 0.0f;
        for (int i = 0; i < b->convexCount; ++i)
        {
            float r2 = b->convexVerts[i].lengthSquared();
            if (r2 > radiusB * radiusB)
                radiusB = r2;
        }
        radiusB = sqrtf(radiusB);
        float minRadius = (radiusA < radiusB ? radiusA : radiusB);
        if (minRadius < 1e-3f)
            return info;

        float sampleStep = 0.4f * minRadius;

        const int kNumSamples = 8;
        const Vector3 sampleDirs[kNumSamples] = {
            t1 * sampleStep,
            t1 * -sampleStep,
            t2 * sampleStep,
            t2 * -sampleStep,
            (t1 + t2) * (sampleStep * 0.7071f),
            (t1 - t2) * (sampleStep * 0.7071f),
            (-t1 + t2) * (sampleStep * 0.7071f),
            (-t1 - t2) * (sampleStep * 0.7071f)};

        const float kDedupDistSq = 0.005f * 0.005f;
        const float kMaxDistSq = (sampleStep * 4.0f) * (sampleStep * 4.0f);

        for (int si = 0; si < kNumSamples && info.contactCount < 4; ++si)
        {
            Vector3 dir = n + sampleDirs[si];
            float dirLen = dir.length();
            if (dirLen < 1e-6f)
                continue;
            dir *= FastMath::fastInvSqrt(dir.lengthSquared());

            Vector3 sa = a->support(dir);
            Vector3 sb = b->support(dir * -1.0f);
            Vector3 m = sa - sb;

            float projN = m.dot(n);
            float pen = -projN;
            if (pen <= 0.0f)
                continue;

            Vector3 contactWorld = (sa + sb) * 0.5f;
            Vector3 delta = contactWorld - info.contacts[0].pos;
            if (delta.lengthSquared() > kMaxDistSq)
                continue;

            bool dup = false;
            for (int j = 0; j < info.contactCount; ++j)
            {
                Vector3 d = info.contacts[j].pos - contactWorld;
                if (d.lengthSquared() < kDedupDistSq)
                {
                    dup = true;
                    break;
                }
            }
            if (dup)
                continue;

            Contact &c = info.contacts[info.contactCount++];
            c.pos = contactWorld;
            c.penetration = pen < epa.depth ? pen : epa.depth;
            c.featureId = makeFeatureId(CONTACT_FEATURE_VERTEX,
                                        static_cast<uint32_t>(si + 1));
        }

        return info;
    }

    inline ContactManifold detectConvexSphere(RigidBody *convex, RigidBody *sphere)
    {
        return detectConvexConvex(convex, sphere);
    }

    __attribute__((always_inline)) inline bool
    pointInConvex(const RigidBody *convex, const Vector3 &pWorld) noexcept
    {
        const Vector3 pLocal = convex->orientation.conjugate().rotate(pWorld - convex->position);

        static const Vector3 kDirs[14] = {
            Vector3(1, 0, 0), Vector3(-1, 0, 0),
            Vector3(0, 1, 0), Vector3(0, -1, 0),
            Vector3(0, 0, 1), Vector3(0, 0, -1),
            Vector3(0.57735f, 0.57735f, 0.57735f),
            Vector3(-0.57735f, 0.57735f, 0.57735f),
            Vector3(0.57735f, -0.57735f, 0.57735f),
            Vector3(-0.57735f, -0.57735f, 0.57735f),
            Vector3(0.57735f, 0.57735f, -0.57735f),
            Vector3(-0.57735f, 0.57735f, -0.57735f),
            Vector3(0.57735f, -0.57735f, -0.57735f),
            Vector3(-0.57735f, -0.57735f, -0.57735f)};

        const float kTol = 1e-4f;
        const int vc = convex->convexCount;

        for (int d = 0; d < 14; ++d)
        {
            float bestDot = -FLT_MAX;
            for (int vi = 0; vi < vc; ++vi)
            {
                float dot = convex->convexVerts[vi].dot(kDirs[d]);
                if (dot > bestDot)
                    bestDot = dot;
            }
            if (pLocal.dot(kDirs[d]) > bestDot + kTol)
                return false;
        }

        const float pLenSq = pLocal.lengthSquared();
        if (pLenSq > 1e-10f)
        {
            Vector3 dir = pLocal * FastMath::fastInvSqrt(pLenSq);
            float bestDot = -FLT_MAX;
            for (int vi = 0; vi < vc; ++vi)
            {
                float dot = convex->convexVerts[vi].dot(dir);
                if (dot > bestDot)
                    bestDot = dot;
            }
            if (sqrtf(pLenSq) > bestDot + kTol)
                return false;
        }

        for (int vi = 0; vi < vc; ++vi)
        {
            float viLenSq = convex->convexVerts[vi].lengthSquared();
            if (viLenSq < 1e-10f)
                continue;
            if (pLocal.dot(convex->convexVerts[vi]) > viLenSq + kTol)
                return false;
        }

        return true;
    }

    inline ContactManifold detectConvexBox(RigidBody *convex, RigidBody *box)
    {
        ContactManifold info;
        if (!convex || !box)
            return info;

        GJKVertex simplex[4];
        int simplexSize = 0;
        bool gjkHit = gjkIntersect(convex, box, simplex, simplexSize);

        EPAResult epa;
        bool epaHit = false;
        if (gjkHit)
            epaHit = epaPenetration(convex, box, simplex, simplexSize, epa);

        if (!epaHit)
        {
            const Vector3 &aMin = convex->bounds.min;
            const Vector3 &aMax = convex->bounds.max;
            const Vector3 &bMin = box->bounds.min;
            const Vector3 &bMax = box->bounds.max;
            const float overlapX = fminf(aMax.x, bMax.x) - fmaxf(aMin.x, bMin.x);
            const float overlapY = fminf(aMax.y, bMax.y) - fmaxf(aMin.y, bMin.y);
            const float overlapZ = fminf(aMax.z, bMax.z) - fmaxf(aMin.z, bMin.z);
            const float kSkin = 0.001f;
            if (overlapX <= kSkin || overlapY <= kSkin || overlapZ <= kSkin)
                return info;

            float pen;
            Vector3 normal;
            if (overlapY <= overlapX && overlapY <= overlapZ)
            {
                pen = overlapY;
                normal = (convex->position.y >= box->position.y)
                             ? Vector3(0, 1, 0)
                             : Vector3(0, -1, 0);
            }
            else if (overlapX <= overlapZ)
            {
                pen = overlapX;
                normal = (convex->position.x >= box->position.x)
                             ? Vector3(1, 0, 0)
                             : Vector3(-1, 0, 0);
            }
            else
            {
                pen = overlapZ;
                normal = (convex->position.z >= box->position.z)
                             ? Vector3(0, 0, 1)
                             : Vector3(0, 0, -1);
            }

            epa.normal = normal;
            epa.depth = pen;
            Vector3 overlapCenter(
                (fmaxf(aMin.x, bMin.x) + fminf(aMax.x, bMax.x)) * 0.5f,
                (fmaxf(aMin.y, bMin.y) + fminf(aMax.y, bMax.y)) * 0.5f,
                (fmaxf(aMin.z, bMin.z) + fminf(aMax.z, bMax.z)) * 0.5f);
            epa.contactPointA = overlapCenter;
            epa.contactPointB = overlapCenter;
            epaHit = true;
        }

        info.hasCollision = true;
        info.bodyA = convex;
        info.bodyB = box;
        info.normal = epa.normal;
        info.contactCount = 0;

        const Quaternion convexRot = convex->orientation;
        const Vector3 convexPos = convex->position;
        const Quaternion boxInvRot = box->orientation.conjugate();
        const Vector3 boxPos = box->position;
        const Vector3 boxHalf = box->size * 0.5f;
        const float kDedupDistSq = 0.005f * 0.005f;

        auto addContact = [&](const Vector3 &contactWorld, float pen, uint32_t featureId) -> bool
        {
            for (int j = 0; j < info.contactCount; ++j)
            {
                Vector3 d = info.contacts[j].pos - contactWorld;
                if (d.lengthSquared() < kDedupDistSq)
                    return false;
            }
            if (info.contactCount >= 4)
                return false;
            Contact &c = info.contacts[info.contactCount++];
            c.pos = contactWorld;
            c.penetration = pen;
            c.featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, featureId);
            return true;
        };

        for (int vi = 0; vi < convex->convexCount && info.contactCount < 4; ++vi)
        {
            Vector3 vWorld = convexPos + convexRot.rotate(convex->convexVerts[vi]);
            Vector3 vLocal = boxInvRot.rotate(vWorld - boxPos);

            const float kSkin = 0.001f;
            if (vLocal.x < -boxHalf.x - kSkin || vLocal.x > boxHalf.x + kSkin)
                continue;
            if (vLocal.y < -boxHalf.y - kSkin || vLocal.y > boxHalf.y + kSkin)
                continue;
            if (vLocal.z < -boxHalf.z - kSkin || vLocal.z > boxHalf.z + kSkin)
                continue;

            float dx = boxHalf.x - fabsf(vLocal.x);
            float dy = boxHalf.y - fabsf(vLocal.y);
            float dz = boxHalf.z - fabsf(vLocal.z);
            float pen;
            if (dx <= dy && dx <= dz)
                pen = dx;
            else if (dy <= dz)
                pen = dy;
            else
                pen = dz;
            if (pen < 0.0f)
                pen = 0.0f;
            if (pen > epa.depth)
                pen = epa.depth;
            if (pen < 1e-5f)
                continue;

            addContact(vWorld, pen, static_cast<uint32_t>(vi + 1));
        }

        if (info.contactCount < 4)
        {
            Vector3 toConvex = convexPos - boxPos;
            Vector3 localToConvex = boxInvRot.rotate(toConvex);

            struct FaceDef
            {
                int axis;
                float sign;
                Vector3 normal;
            };
            const FaceDef faces[6] = {
                {0, 1, Vector3(1, 0, 0)},
                {0, -1, Vector3(-1, 0, 0)},
                {1, 1, Vector3(0, 1, 0)},
                {1, -1, Vector3(0, -1, 0)},
                {2, 1, Vector3(0, 0, 1)},
                {2, -1, Vector3(0, 0, -1)}};

            for (int fi = 0; fi < 6 && info.contactCount < 4; ++fi)
            {
                const FaceDef &f = faces[fi];
                if (f.normal.dot(localToConvex) <= 0.0f)
                    continue;

                const float faceU = (f.axis == 0) ? boxHalf.y : boxHalf.x;
                const float faceV = (f.axis == 2) ? boxHalf.y : boxHalf.z;
                const float faceN = (f.axis == 0 ? boxHalf.x : (f.axis == 1 ? boxHalf.y : boxHalf.z)) * f.sign;

                for (int su = -1; su <= 1 && info.contactCount < 4; ++su)
                {
                    for (int sv = -1; sv <= 1 && info.contactCount < 4; ++sv)
                    {
                        Vector3 pLocal(0, 0, 0);
                        float uOff = faceU * 0.5f * static_cast<float>(su);
                        float vOff = faceV * 0.5f * static_cast<float>(sv);
                        if (f.axis == 0)
                        {
                            pLocal.x = faceN;
                            pLocal.y = uOff;
                            pLocal.z = vOff;
                        }
                        else if (f.axis == 1)
                        {
                            pLocal.x = uOff;
                            pLocal.y = faceN;
                            pLocal.z = vOff;
                        }
                        else
                        {
                            pLocal.x = uOff;
                            pLocal.y = vOff;
                            pLocal.z = faceN;
                        }

                        Vector3 pWorld = box->orientation.rotate(pLocal) + boxPos;
                        if (!pointInConvex(convex, pWorld))
                            continue;

                        addContact(pWorld, epa.depth,
                                   100u + static_cast<uint32_t>(fi * 9 + (su + 1) * 3 + (sv + 1)));
                    }
                }
            }
        }

        if (info.contactCount == 0)
        {
            info.contactCount = 1;
            info.contacts[0].pos = (epa.contactPointA + epa.contactPointB) * 0.5f;
            info.contacts[0].penetration = epa.depth;
            info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, 0u);
        }

        return info;
    }
}

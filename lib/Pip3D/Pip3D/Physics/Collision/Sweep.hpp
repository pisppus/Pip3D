#pragma once

#include "Physics/RigidBody/Body.hpp"
#include "Physics/RigidBody/Contacts.hpp"

namespace pip3D
{
    inline ContactManifold predictContacts(RigidBody *a, RigidBody *b, float deltaTime)
    {
        ContactManifold info;
        if (!a || !b)
            return info;
        if (deltaTime <= 0.0f)
            return info;

        bool aImmobile = (a->isStatic || a->isKinematic || a->isSleeping);
        bool bImmobile = (b->isStatic || b->isKinematic || b->isSleeping);
        if (aImmobile && bImmobile)
            return info;

        Vector3 relVelCheck = b->velocity - a->velocity;
        if (relVelCheck.lengthSquared() < 4.0f)
            return info;

        Vector3 dispA = a->velocity * deltaTime;
        Vector3 dispB = b->velocity * deltaTime;

        AABB sweptA = a->bounds;
        AABB sweptB = b->bounds;
        if (dispA.x < 0.0f)
            sweptA.min.x += dispA.x;
        else
            sweptA.max.x += dispA.x;
        if (dispA.y < 0.0f)
            sweptA.min.y += dispA.y;
        else
            sweptA.max.y += dispA.y;
        if (dispA.z < 0.0f)
            sweptA.min.z += dispA.z;
        else
            sweptA.max.z += dispA.z;
        if (dispB.x < 0.0f)
            sweptB.min.x += dispB.x;
        else
            sweptB.max.x += dispB.x;
        if (dispB.y < 0.0f)
            sweptB.min.y += dispB.y;
        else
            sweptB.max.y += dispB.y;
        if (dispB.z < 0.0f)
            sweptB.min.z += dispB.z;
        else
            sweptB.max.z += dispB.z;

        if (!sweptA.intersects(sweptB))
            return info;

        float gapX = (a->position.x <= b->position.x)
                         ? (b->bounds.min.x - a->bounds.max.x)
                         : (a->bounds.min.x - b->bounds.max.x);
        float gapY = (a->position.y <= b->position.y)
                         ? (b->bounds.min.y - a->bounds.max.y)
                         : (a->bounds.min.y - b->bounds.max.y);
        float gapZ = (a->position.z <= b->position.z)
                         ? (b->bounds.min.z - a->bounds.max.z)
                         : (a->bounds.min.z - b->bounds.max.z);

        if (gapX <= 0.0f && gapY <= 0.0f && gapZ <= 0.0f)
            return info;

        struct AxisGap
        {
            int axis;
            float gap;
        };
        AxisGap candidates[3] = {{0, gapX}, {1, gapY}, {2, gapZ}};

        int bestAxis = -1;
        float bestGap = FLT_MAX;
        for (int i = 0; i < 3; ++i)
        {
            if (candidates[i].gap > 0.0f && candidates[i].gap < bestGap)
            {
                bestGap = candidates[i].gap;
                bestAxis = candidates[i].axis;
            }
        }
        if (bestAxis < 0)
            return info;

        Vector3 relVel = b->velocity - a->velocity;
        Vector3 normal(0.0f, 0.0f, 0.0f);
        if (bestAxis == 0)
            normal = (a->position.x <= b->position.x) ? Vector3(1, 0, 0) : Vector3(-1, 0, 0);
        else if (bestAxis == 1)
            normal = (a->position.y <= b->position.y) ? Vector3(0, 1, 0) : Vector3(0, -1, 0);
        else
            normal = (a->position.z <= b->position.z) ? Vector3(0, 0, 1) : Vector3(0, 0, -1);

        float vn = relVel.dot(normal);
        if (vn >= -0.5f)
            return info;

        float closingSpeed = -vn;
        float toi = bestGap / closingSpeed;
        if (toi < 0.0f || toi > deltaTime)
            return info;

        info.hasCollision = true;
        info.bodyA = a;
        info.bodyB = b;
        info.normal = normal;
        info.contactCount = 1;
        info.contacts[0].pos = (a->position + b->position) * 0.5f;
        info.contacts[0].penetration = -bestGap;
        info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, 0u);

        return info;
    }
}

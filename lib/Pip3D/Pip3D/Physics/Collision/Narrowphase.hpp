#pragma once

#include <float.h>
#include <math.h>

#include "Math/Collision.hpp"
#include "../Dynamics/Body.hpp"
#include "../Dynamics/Contacts.hpp"
#include "../Types.hpp"
#include "GJK.hpp"
#include "Simplex.hpp"
#include "EPA.hpp"
#include "Helpers.hpp"
#include "Primitives.hpp"
#include "Convex.hpp"

namespace pip3D
{
    inline ContactManifold detectSphereSphere(RigidBody *a, RigidBody *b, float currentDeltaTime);
    inline ContactManifold detectSphereBox(RigidBody *sphere, RigidBody *box, float currentDeltaTime);
    inline ContactManifold detectBoxBox(RigidBody *a, RigidBody *b);
    inline ContactManifold detectCapsuleSphere(RigidBody *cap, RigidBody *sphere);
    inline ContactManifold detectCapsuleBox(RigidBody *cap, RigidBody *box);
    inline ContactManifold detectCapsuleCapsule(RigidBody *a, RigidBody *b);
    inline ContactManifold detectCollision(RigidBody *a, RigidBody *b, float currentDeltaTime)
    {
        ContactManifold info;
        if (!a || !b)
            return info;

        bool bothStatic = a->isStatic && b->isStatic;
        bool bothKinematicNonTrigger = a->isKinematic && b->isKinematic && !a->isTrigger && !b->isTrigger;
        if (bothStatic || bothKinematicNonTrigger)
            return info;

        if (!a->bounds.intersects(b->bounds))
            return info;

        BodyShape sa = a->shape;
        BodyShape sb = b->shape;

        auto adoptSwapped = [&](ContactManifold swapped) -> ContactManifold &
        {
            if (!swapped.hasCollision)
                return info;
            info.hasCollision = true;
            info.bodyA = a;
            info.bodyB = b;
            info.normal = swapped.normal * -1.0f;
            info.contactCount = swapped.contactCount;
            for (int i = 0; i < swapped.contactCount && i < 4; ++i)
            {
                info.contacts[i].pos = swapped.contacts[i].pos;
                info.contacts[i].penetration = swapped.contacts[i].penetration;
                info.contacts[i].featureId = swapped.contacts[i].featureId;
            }
            return info;
        };

        if (sa == BODY_SHAPE_SPHERE && sb == BODY_SHAPE_SPHERE)
            return detectSphereSphere(a, b, currentDeltaTime);

        if (sa == BODY_SHAPE_SPHERE && sb == BODY_SHAPE_BOX)
            return detectSphereBox(a, b, currentDeltaTime);
        if (sa == BODY_SHAPE_BOX && sb == BODY_SHAPE_SPHERE)
            return adoptSwapped(detectSphereBox(b, a, currentDeltaTime));

        if (sa == BODY_SHAPE_BOX && sb == BODY_SHAPE_BOX)
            return detectBoxBox(a, b);

        if (sa == BODY_SHAPE_CAPSULE && sb == BODY_SHAPE_SPHERE)
            return detectCapsuleSphere(a, b);
        if (sa == BODY_SHAPE_SPHERE && sb == BODY_SHAPE_CAPSULE)
            return adoptSwapped(detectCapsuleSphere(b, a));

        if (sa == BODY_SHAPE_CAPSULE && sb == BODY_SHAPE_BOX)
            return detectCapsuleBox(a, b);
        if (sa == BODY_SHAPE_BOX && sb == BODY_SHAPE_CAPSULE)
            return adoptSwapped(detectCapsuleBox(b, a));

        if (sa == BODY_SHAPE_CAPSULE && sb == BODY_SHAPE_CAPSULE)
            return detectCapsuleCapsule(a, b);

        if (sa == BODY_SHAPE_CONVEX && sb == BODY_SHAPE_BOX)
            return detectConvexBox(a, b);
        if (sa == BODY_SHAPE_BOX && sb == BODY_SHAPE_CONVEX)
            return adoptSwapped(detectConvexBox(b, a));

        if (sa == BODY_SHAPE_CONVEX || sb == BODY_SHAPE_CONVEX)
            return detectConvexConvex(a, b);

        return info;
    }
}

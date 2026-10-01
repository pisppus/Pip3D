#pragma once

#include <float.h>
#include <math.h>

#include "Math/Collision.hpp"
#include "Physics/RigidBody/Body.hpp"
#include "Physics/RigidBody/Contacts.hpp"
#include "Physics/Types.hpp"
#include "Physics/Collision/GJK.hpp"
#include "Physics/Collision/Simplex.hpp"
#include "Physics/Collision/EPA.hpp"
#include "Physics/Collision/Helpers.hpp"
#include "Physics/Collision/Primitives.hpp"
#include "Physics/Collision/Convex.hpp"

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
        info.hasCollision = false;
        info.hasRealContact = false;
        info.bodyA = a;
        info.bodyB = b;
        info.contactCount = 0;
        info.normal = Vector3(0, 1, 0);
        for (int i = 0; i < PhysicsConfig::MAX_CONTACT_POINTS; ++i)
        {
            info.contacts[i].pos = Vector3(0, 0, 0);
            info.contacts[i].localPointA = Vector3(0, 0, 0);
            info.contacts[i].localPointB = Vector3(0, 0, 0);
            info.contacts[i].penetration = 0.0f;
            info.contacts[i].featureId = 0;
            info.contacts[i].lifetime = 0;
        }

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

        const bool aConvexLike = (sa == BODY_SHAPE_CONVEX || sa == BODY_SHAPE_BOX || sa == BODY_SHAPE_CYLINDER);
        const bool bConvexLike = (sb == BODY_SHAPE_CONVEX || sb == BODY_SHAPE_BOX || sb == BODY_SHAPE_CYLINDER);

        if (aConvexLike && bConvexLike)
            return detectConvexConvex(a, b);

        auto adoptSwapped = [&](ContactManifold swapped) -> ContactManifold &
        {
            if (!swapped.hasCollision)
                return info;
            info = swapped;
            info.bodyA = a;
            info.bodyB = b;
            info.normal = swapped.normal * -1.0f;
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

        if (aConvexLike && sb == BODY_SHAPE_SPHERE)
            return detectConvexConvex(a, b);
        if (sa == BODY_SHAPE_SPHERE && bConvexLike)
            return adoptSwapped(detectConvexConvex(b, a));

        if (aConvexLike && sb == BODY_SHAPE_CAPSULE)
            return detectConvexConvex(a, b);
        if (sa == BODY_SHAPE_CAPSULE && bConvexLike)
            return adoptSwapped(detectConvexConvex(b, a));

        return info;
    }
}

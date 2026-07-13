#pragma once

#include "Math/Collision.hpp"
#include "../Types.hpp"

namespace pip3D
{
    struct RigidBody;

    struct Contact
    {

        Vector3 pos;
        float penetration;

        float accumulatedImpulse;
        float tangentImpulse1;
        float tangentImpulse2;

        float normalMass;
        float tangentMass1;
        float tangentMass2;

        float bias;
        float restitution;
        float friction;

        Vector3 tangent1;
        Vector3 tangent2;

        uint32_t featureId;

        Contact()
            : pos(0.0f, 0.0f, 0.0f),
              penetration(0.0f),
              accumulatedImpulse(0.0f),
              tangentImpulse1(0.0f),
              tangentImpulse2(0.0f),
              normalMass(0.0f),
              tangentMass1(0.0f),
              tangentMass2(0.0f),
              bias(0.0f),
              restitution(0.0f),
              friction(0.0f),
              tangent1(1.0f, 0.0f, 0.0f),
              tangent2(0.0f, 1.0f, 0.0f),
              featureId(0) {}
    };

    struct ContactManifold
    {
        bool hasCollision;
        Vector3 normal;
        Contact contacts[4];
        int contactCount;
        RigidBody *bodyA;
        RigidBody *bodyB;

        ContactManifold()
            : hasCollision(false),
              normal(0.0f, 1.0f, 0.0f),
              contactCount(0),
              bodyA(nullptr),
              bodyB(nullptr) {}
    };
}

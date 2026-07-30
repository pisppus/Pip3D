#pragma once

#include "Math/Collision.hpp"
#include "Physics/Types.hpp"

namespace pip3D
{
    struct RigidBody;
    struct RaycastHit
    {
        bool hit;
        Vector3 point;
        Vector3 normal;
        float distance;
        RigidBody *body;

        RaycastHit()
            : hit(false),
              point(0.0f, 0.0f, 0.0f),
              normal(0.0f, 1.0f, 0.0f),
              distance(0.0f),
              body(nullptr) {}
    };
}

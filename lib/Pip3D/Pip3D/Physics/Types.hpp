#pragma once

#include "Core/Platform.hpp"

namespace pip3D
{

    enum BodyShape
    {
        BODY_SHAPE_SPHERE = 0,
        BODY_SHAPE_CAPSULE = 1,
        BODY_SHAPE_BOX = 2,
        BODY_SHAPE_CONVEX = 3
    };

    struct PhysicsMaterial
    {
        float friction;
        float restitution;

        PhysicsMaterial()
            : friction(0.5f), restitution(0.5f) {}

        PhysicsMaterial(float f, float r)
            : friction(f), restitution(r) {}
    };

    enum ContactFeatureKind : uint8_t
    {
        CONTACT_FEATURE_VERTEX = 0x01,
        CONTACT_FEATURE_FACE = 0x02,
        CONTACT_FEATURE_EDGE = 0x03
    };

    constexpr uint32_t makeFeatureId(ContactFeatureKind kind, uint32_t key)
    {
        return (static_cast<uint32_t>(kind) << 24) | (key & 0x00FFFFFFu);
    }

    namespace PhysicsConfig
    {

        static constexpr int SOLVER_ITERATIONS = 15;
        static constexpr int MAX_SUBSTEPS = 8;

        static constexpr float BAUMGARTE = 0.2f;
        static constexpr float POSITION_SLOP = 0.005f;
        static constexpr float POSITION_PERCENT = 0.8f;

        static constexpr float SLEEP_LINEAR_THRESHOLD_SQ = 1e-3f;
        static constexpr float SLEEP_ANGULAR_THRESHOLD_SQ = 1e-3f;
        static constexpr float SLEEP_TIME = 0.5f;
        static constexpr float WAKE_LINEAR_THRESHOLD_SQ = 1e-2f;
        static constexpr float WAKE_ANGULAR_THRESHOLD_SQ = 1e-2f;

        static constexpr float RESTING_LINEAR_ZERO_SQ = 1e-2f;
        static constexpr float RESTING_ANGULAR_ZERO_SQ = 1e-2f;

        static constexpr float RESTITUTION_THRESHOLD = 0.5f;

        static constexpr float DEFAULT_FIXED_TIMESTEP = 1.0f / 120.0f;

        static constexpr float LINEAR_DAMPING = 0.05f;
        static constexpr float ANGULAR_DAMPING = 0.4f;

        static constexpr float MAX_LINEAR_VELOCITY = 40.0f;
        static constexpr float MAX_ANGULAR_VELOCITY = 15.0f;

        static constexpr float MANIFOLD_CONTACT_EPS = 0.01f;

        static constexpr float CONTACT_SKIN = 0.005f;

        static constexpr float WAKE_PROPAGATION_VEL_SQ = 1e-4f;
    }
}

#pragma once

#include "Math/Collision.hpp"
#include "../Types.hpp"

namespace pip3D
{
    struct RigidBody;

    struct Contact
    {
        Vector3 pos;
        Vector3 localPointA;
        Vector3 localPointB;
        float penetration;
        float accumulatedImpulse;
        float tangentImpulse1;
        float tangentImpulse2;
        float normalMass;
        float tangentMass1;
        float tangentMass2;
        float bias;
        uint16_t material;
        uint16_t lifetime;
        uint32_t featureId;
    };

    PIP3D_FORCE_INLINE uint8_t packMaterialChannel(float v) noexcept
    {
        if (v < 0.0f)
            v = 0.0f;
        if (v > 1.0f)
            v = 1.0f;
        return static_cast<uint8_t>(v * 255.0f + 0.5f);
    }
    PIP3D_FORCE_INLINE float unpackMaterialChannel(uint8_t v) noexcept
    {
        return static_cast<float>(v) * (1.0f / 255.0f);
    }
    PIP3D_FORCE_INLINE uint16_t packMaterial(float restitution, float friction) noexcept
    {
        return (static_cast<uint16_t>(packMaterialChannel(restitution)) << 8) | static_cast<uint16_t>(packMaterialChannel(friction));
    }
    PIP3D_FORCE_INLINE float unpackRestitution(uint16_t m) noexcept
    {
        return unpackMaterialChannel(static_cast<uint8_t>(m >> 8));
    }
    PIP3D_FORCE_INLINE float unpackFriction(uint16_t m) noexcept
    {
        return unpackMaterialChannel(static_cast<uint8_t>(m & 0xFF));
    }

    struct ContactManifold
    {
        bool hasCollision;
        bool hasRealContact;
        uint8_t bodyAIndex;
        uint8_t bodyBIndex;
        Vector3 normal;
        Contact contacts[PhysicsConfig::MAX_CONTACT_POINTS];
        int contactCount;
        RigidBody *bodyA;
        RigidBody *bodyB;
    };

}

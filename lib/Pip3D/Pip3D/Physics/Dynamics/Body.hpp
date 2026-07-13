#pragma once

#include "Math/Collision.hpp"
#include "../Types.hpp"

namespace pip3D
{
    struct __attribute__((aligned(16))) RigidBody
    {

        Vector3 position;
        Vector3 previousPosition;
        Vector3 velocity;
        Vector3 acceleration;
        Vector3 angularVelocity;
        Quaternion orientation;
        Quaternion previousOrientation;

        Vector3 posCorrectionVelocity;
        Vector3 posCorrectionAngular;

        Vector3 renderPosition;
        Quaternion renderOrientation;

        Vector3 size;
        float radius;
        float capsuleHalfHeight;
        BodyShape shape;

        static constexpr int kMaxConvexVerts = 32;
        Vector3 convexVerts[kMaxConvexVerts];
        int convexCount;

        float mass;
        float invMass;
        Vector3 invInertia;
        float worldInvInertia[9];

        float restitution;
        float friction;

        float linearDamping;
        float angularDamping;
        float gravityScale;

        bool isStatic;
        bool isKinematic;
        bool isTrigger;
        bool canSleep;
        bool isSleeping;
        float sleepTimer;

        AABB bounds;

        RigidBody()
            : position(0, 0, 0), previousPosition(0, 0, 0),
              velocity(0, 0, 0), acceleration(0, 0, 0),
              angularVelocity(0, 0, 0), orientation(), previousOrientation(),
              posCorrectionVelocity(0, 0, 0), posCorrectionAngular(0, 0, 0),
              renderPosition(0, 0, 0), renderOrientation(),
              size(1, 1, 1), radius(0.5f), capsuleHalfHeight(0.0f), shape(BODY_SHAPE_BOX),
              convexCount(0),
              mass(1.0f), invMass(1.0f),
              invInertia(1.0f, 1.0f, 1.0f),
              restitution(0.5f), friction(0.5f),
              linearDamping(PhysicsConfig::LINEAR_DAMPING),
              angularDamping(PhysicsConfig::ANGULAR_DAMPING),
              gravityScale(1.0f),
              isStatic(false), isKinematic(false), isTrigger(false),
              canSleep(true), isSleeping(false), sleepTimer(0.0f),
              bounds(AABB::fromCenterSize(Vector3(0, 0, 0), Vector3(1, 1, 1)))
        {
            for (int i = 0; i < kMaxConvexVerts; ++i)
                convexVerts[i] = Vector3(0, 0, 0);
            computeInertia();
            updateWorldInvInertia();
        }

        RigidBody(const Vector3 &pos, const Vector3 &size_, float m = 1.0f)
            : position(pos), previousPosition(pos),
              velocity(0, 0, 0), acceleration(0, 0, 0),
              angularVelocity(0, 0, 0), orientation(), previousOrientation(),
              posCorrectionVelocity(0, 0, 0), posCorrectionAngular(0, 0, 0),
              renderPosition(pos), renderOrientation(),
              size(size_), radius(size_.x * 0.5f), capsuleHalfHeight(0.0f), shape(BODY_SHAPE_BOX),
              convexCount(0),
              mass(m), invMass(m > 0.0f ? 1.0f / m : 0.0f),
              invInertia(1.0f, 1.0f, 1.0f),
              restitution(0.5f), friction(0.5f),
              linearDamping(PhysicsConfig::LINEAR_DAMPING),
              angularDamping(PhysicsConfig::ANGULAR_DAMPING),
              gravityScale(1.0f),
              isStatic(false), isKinematic(false), isTrigger(false),
              canSleep(true), isSleeping(false), sleepTimer(0.0f),
              bounds(AABB::fromCenterSize(pos, size_))
        {
            for (int i = 0; i < kMaxConvexVerts; ++i)
                convexVerts[i] = Vector3(0, 0, 0);
            computeInertia();
            updateWorldInvInertia();
        }

        void setBox(const Vector3 &newSize);
        void setSphere(float r);
        void setCapsule(float r, float halfHeight);
        void setConvex(const Vector3 *verts, int count);
        Vector3 support(const Vector3 &dirWorld) const noexcept;
        void updateBoundsFromTransform();

        __attribute__((always_inline)) inline Vector3
        mulWorldInvInertia(const Vector3 &v) const noexcept
        {
            return Vector3(
                worldInvInertia[0] * v.x + worldInvInertia[1] * v.y + worldInvInertia[2] * v.z,
                worldInvInertia[3] * v.x + worldInvInertia[4] * v.y + worldInvInertia[5] * v.z,
                worldInvInertia[6] * v.x + worldInvInertia[7] * v.y + worldInvInertia[8] * v.z);
        }

        __attribute__((always_inline)) inline void updateWorldInvInertia() noexcept
        {
            if (invMass <= 0.0f)
            {
                for (int i = 0; i < 9; ++i)
                    worldInvInertia[i] = 0.0f;
                return;
            }

            const float d0 = invInertia.x;
            const float d1 = invInertia.y;
            const float d2 = invInertia.z;

            Vector3 ex = orientation.rotate(Vector3(1.0f, 0.0f, 0.0f));
            Vector3 ey = orientation.rotate(Vector3(0.0f, 1.0f, 0.0f));
            Vector3 ez = orientation.rotate(Vector3(0.0f, 0.0f, 1.0f));

            worldInvInertia[0] = d0 * ex.x * ex.x + d1 * ey.x * ey.x + d2 * ez.x * ez.x;
            worldInvInertia[1] = d0 * ex.x * ex.y + d1 * ey.x * ey.y + d2 * ez.x * ey.y;
            worldInvInertia[2] = d0 * ex.x * ex.z + d1 * ey.x * ey.z + d2 * ez.x * ez.z;

            worldInvInertia[3] = worldInvInertia[1];
            worldInvInertia[4] = d0 * ex.y * ex.y + d1 * ey.y * ey.y + d2 * ez.y * ez.y;
            worldInvInertia[5] = d0 * ex.y * ex.z + d1 * ey.y * ey.z + d2 * ez.y * ez.z;

            worldInvInertia[6] = worldInvInertia[2];
            worldInvInertia[7] = worldInvInertia[5];
            worldInvInertia[8] = d0 * ex.z * ex.z + d1 * ey.z * ey.z + d2 * ez.z * ez.z;
        }

        __attribute__((always_inline)) inline void applyForce(const Vector3 &force)
        {
            if (!isStatic && !isKinematic && mass > 0.0f)
            {
                acceleration += force * invMass;
                isSleeping = false;
                sleepTimer = 0.0f;
            }
        }

        __attribute__((always_inline)) inline void applyImpulse(const Vector3 &impulse)
        {
            if (!isStatic && !isKinematic && invMass > 0.0f)
            {
                velocity += impulse * invMass;
                isSleeping = false;
                sleepTimer = 0.0f;
            }
        }

        __attribute__((always_inline)) inline void
        applyImpulseAt(const Vector3 &impulse, const Vector3 &worldPoint)
        {
            if (!isStatic && !isKinematic && invMass > 0.0f)
            {
                Vector3 r = worldPoint - position;
                velocity += impulse * invMass;
                angularVelocity += mulWorldInvInertia(r.cross(impulse));
                isSleeping = false;
                sleepTimer = 0.0f;
            }
        }

        void beginStep();
        void update(float deltaTime);
        void integratePseudoVelocity(float deltaTime);
        void interpolateTransforms(float alpha);

        __attribute__((always_inline)) inline void setPosition(const Vector3 &pos)
        {
            position = pos;
            previousPosition = pos;
            updateBoundsFromTransform();
            isSleeping = false;
            sleepTimer = 0.0f;
        }

        __attribute__((always_inline)) inline void setOrientation(const Quaternion &q)
        {
            orientation = q;
            previousOrientation = q;
            updateWorldInvInertia();
            updateBoundsFromTransform();
            isSleeping = false;
            sleepTimer = 0.0f;
        }

        __attribute__((always_inline)) inline void setStatic(bool s)
        {
            isStatic = s;
            if (isStatic)
            {
                isKinematic = false;
                isTrigger = false;
                velocity = Vector3(0, 0, 0);
                acceleration = Vector3(0, 0, 0);
                angularVelocity = Vector3(0, 0, 0);
                isSleeping = false;
                sleepTimer = 0.0f;
            }
            computeInertia();
            updateWorldInvInertia();
        }

        __attribute__((always_inline)) inline void setKinematic(bool k)
        {
            isKinematic = k;
            if (isKinematic)
            {
                isStatic = false;
                canSleep = false;
                isSleeping = false;
                sleepTimer = 0.0f;
                velocity = Vector3(0, 0, 0);
                acceleration = Vector3(0, 0, 0);
                angularVelocity = Vector3(0, 0, 0);
            }
            computeInertia();
            updateWorldInvInertia();
        }

        __attribute__((always_inline)) inline void setTrigger(bool t) { isTrigger = t; }

        __attribute__((always_inline)) inline void wakeUp()
        {
            if (isSleeping)
            {
                isSleeping = false;
                sleepTimer = 0.0f;
            }
        }

        __attribute__((always_inline)) inline void setCanSleep(bool value)
        {
            canSleep = value;
            if (!canSleep)
            {
                isSleeping = false;
                sleepTimer = 0.0f;
            }
        }

        __attribute__((always_inline)) inline void setMaterial(const PhysicsMaterial &m)
        {
            friction = m.friction;
            restitution = m.restitution;
        }

    private:
        __attribute__((always_inline)) inline void computeInertia()
        {
            if (isStatic || isKinematic || mass <= 0.0f)
            {
                invMass = 0.0f;
                invInertia = Vector3(0, 0, 0);
                return;
            }
            invMass = 1.0f / mass;

            if (shape == BODY_SHAPE_BOX)
            {
                float hx = size.x * 0.5f;
                float hy = size.y * 0.5f;
                float hz = size.z * 0.5f;
                float ix = (mass / 12.0f) * (hy * hy + hz * hz);
                float iy = (mass / 12.0f) * (hx * hx + hz * hz);
                float iz = (mass / 12.0f) * (hx * hx + hy * hy);
                invInertia.x = ix > 0.0f ? 1.0f / ix : 0.0f;
                invInertia.y = iy > 0.0f ? 1.0f / iy : 0.0f;
                invInertia.z = iz > 0.0f ? 1.0f / iz : 0.0f;
            }
            else if (shape == BODY_SHAPE_CAPSULE)
            {
                float r = radius;
                float L = capsuleHalfHeight * 2.0f;

                float iyy = 0.5f * mass * r * r;
                float ixz = mass * (r * r * 0.25f + L * L / 12.0f);
                invInertia.x = ixz > 0.0f ? 1.0f / ixz : 0.0f;
                invInertia.y = iyy > 0.0f ? 1.0f / iyy : 0.0f;
                invInertia.z = ixz > 0.0f ? 1.0f / ixz : 0.0f;
            }
            else if (shape == BODY_SHAPE_CONVEX)
            {

                float hx = size.x * 0.5f;
                float hy = size.y * 0.5f;
                float hz = size.z * 0.5f;
                float ix = (mass / 12.0f) * (hy * hy + hz * hz);
                float iy = (mass / 12.0f) * (hx * hx + hz * hz);
                float iz = (mass / 12.0f) * (hx * hx + hy * hy);
                invInertia.x = ix > 0.0f ? 1.0f / ix : 0.0f;
                invInertia.y = iy > 0.0f ? 1.0f / iy : 0.0f;
                invInertia.z = iz > 0.0f ? 1.0f / iz : 0.0f;
            }
            else
            {
                float r = radius;
                float i = 0.4f * mass * r * r;
                float invI = i > 0.0f ? 1.0f / i : 0.0f;
                invInertia = Vector3(invI, invI, invI);
            }
        }
    };
}

#include "Collider.hpp"
#include "Integration.hpp"
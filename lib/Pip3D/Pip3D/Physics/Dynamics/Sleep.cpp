#include "../World.hpp"

#include <cmath>

namespace pip3D
{
    void PhysicsWorld::maybeWake(ContactManifold &info)
    {
        RigidBody *a = info.bodyA;
        RigidBody *b = info.bodyB;
        if (!a->isSleeping && !b->isSleeping)
            return;

        Vector3 vRel = b->velocity - a->velocity;
        float vRelSq = vRel.lengthSquared();
        if (vRelSq <= PhysicsConfig::WAKE_LINEAR_THRESHOLD_SQ)
            return;

        if (a->isSleeping)
            a->wakeUp();
        if (b->isSleeping)
            b->wakeUp();
    }

    void PhysicsWorld::applyRestingVelocityZeroing()
    {
        if (contactConstraints.empty())
            return;

        const size_t bodyCount = bodies.size();
        const size_t contactCount = contactConstraints.size();

        for (size_t i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies[i];
            if (!b || b->isStatic || b->isKinematic || b->isSleeping)
                continue;

            Vector3 normals[16];
            int normalCount = 0;

            for (size_t c = 0; c < contactCount && normalCount < 16; ++c)
            {
                const ContactManifold &info = contactConstraints[c];
                bool hasReal = false;
                for (int k = 0; k < info.contactCount; ++k)
                    if (info.contacts[k].penetration > 0.0f)
                    {
                        hasReal = true;
                        break;
                    }
                if (!hasReal)
                    continue;

                if (info.bodyA == b)
                    normals[normalCount++] = info.normal * -1.0f;
                else if (info.bodyB == b)
                    normals[normalCount++] = info.normal;
            }
            if (normalCount == 0)
                continue;

            float v2 = b->velocity.lengthSquared();
            if (v2 < PhysicsConfig::RESTING_LINEAR_ZERO_SQ * 4.0f)
            {
                for (int n = 0; n < normalCount; ++n)
                {
                    float vn = b->velocity.dot(normals[n]);

                    if (vn < 0.0f && vn * vn < PhysicsConfig::RESTING_LINEAR_ZERO_SQ)
                    {

                        b->velocity -= normals[n] * vn;
                    }
                }
            }

            float w2 = b->angularVelocity.lengthSquared();
            if (w2 < PhysicsConfig::RESTING_ANGULAR_ZERO_SQ)
                b->angularVelocity = Vector3(0, 0, 0);
        }
    }

    void PhysicsWorld::updateSleepAndSettle(float deltaTime)
    {
        const size_t bodyCount = bodies.size();

        bool supported[256] = {false};
        bool hasDeepPen[256] = {false};
        const size_t contactCount = contactConstraints.size();
        const size_t cap = (bodyCount < 256) ? bodyCount : 256;
        const float kSleepPenLimit = 2.0f * PhysicsConfig::POSITION_SLOP;

        for (size_t c = 0; c < contactCount; ++c)
        {
            const ContactManifold &info = contactConstraints[c];
            bool hasReal = false;
            float maxPen = 0.0f;
            for (int k = 0; k < info.contactCount; ++k)
            {
                if (info.contacts[k].penetration > 0.0f)
                    hasReal = true;
                if (info.contacts[k].penetration > maxPen)
                    maxPen = info.contacts[k].penetration;
            }
            if (!hasReal)
                continue;

            RigidBody *a = info.bodyA;
            RigidBody *b = info.bodyB;

            if (b && (b->isStatic || b->isSleeping))
            {
                for (size_t i = 0; i < cap; ++i)
                    if (bodies[i] == a)
                    {
                        supported[i] = true;
                        break;
                    }
            }

            if (a && (a->isStatic || a->isSleeping))
            {
                for (size_t i = 0; i < cap; ++i)
                    if (bodies[i] == b)
                    {
                        supported[i] = true;
                        break;
                    }
            }

            if (maxPen > kSleepPenLimit)
            {
                for (size_t i = 0; i < cap; ++i)
                {
                    if (bodies[i] == a || bodies[i] == b)
                        hasDeepPen[i] = true;
                }
            }
        }

        for (size_t i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies[i];
            if (b->isStatic)
                continue;
            if (b->isKinematic)
                continue;
            if (!b->canSleep)
                continue;

            float v2 = b->velocity.lengthSquared();
            float w2 = b->angularVelocity.lengthSquared();

            if (v2 > PhysicsConfig::WAKE_LINEAR_THRESHOLD_SQ ||
                w2 > PhysicsConfig::WAKE_ANGULAR_THRESHOLD_SQ)
            {
                b->sleepTimer = 0.0f;
                b->isSleeping = false;
                continue;
            }

            if (v2 < PhysicsConfig::SLEEP_LINEAR_THRESHOLD_SQ &&
                w2 < PhysicsConfig::SLEEP_ANGULAR_THRESHOLD_SQ)
            {

                if (i < cap && !supported[i])
                {
                    b->sleepTimer = 0.0f;
                    continue;
                }

                if (i < cap && hasDeepPen[i])
                {
                    b->sleepTimer = 0.0f;
                    continue;
                }

                b->sleepTimer += deltaTime;
                if (b->sleepTimer > PhysicsConfig::SLEEP_TIME)
                {
                    b->isSleeping = true;
                    b->velocity = Vector3(0, 0, 0);
                    b->angularVelocity = Vector3(0, 0, 0);
                    b->acceleration = Vector3(0, 0, 0);
                }
            }
        }
    }

}

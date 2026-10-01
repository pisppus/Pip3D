#pragma once

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Physics/RigidBody/Body.hpp"
#include "Physics/RigidBody/Contacts.hpp"
#include "Physics/Types.hpp"

namespace pip3D
{

    class Resolver
    {
    public:
        PIP3D_FORCE_INLINE static void
        buildTangentBasis(const Vector3 &n, Vector3 &t1, Vector3 &t2) noexcept
        {
            if (fabsf(n.x) <= 0.70710678f)
                t1 = Vector3(1.0f - n.x * n.x, -n.y * n.x, -n.z * n.x);
            else
                t1 = Vector3(-n.x * n.y, 1.0f - n.y * n.y, -n.z * n.y);
            t1.normalize();
            t2 = n.cross(t1);
        }

        PIP3D_FORCE_INLINE static float
        effectiveMass(const RigidBody *a, const RigidBody *b,
                      const Vector3 &rA, const Vector3 &rB,
                      const Vector3 &d, float invMassA,
                      float invMassB) noexcept
        {
            const Vector3 rAxd = rA.cross(d);
            const Vector3 rBxd = rB.cross(d);
            const Vector3 invIA = invMassA > 0.0f
                                      ? a->mulWorldInvInertia(rAxd)
                                      : Vector3(0.0f, 0.0f, 0.0f);
            const Vector3 invIB = invMassB > 0.0f
                                      ? b->mulWorldInvInertia(rBxd)
                                      : Vector3(0.0f, 0.0f, 0.0f);
            const float angTerm = invIA.cross(rA).dot(d) +
                                  invIB.cross(rB).dot(d);
            const float denom = invMassA + invMassB + angTerm;
            return denom > 0.0f ? 1.0f / denom : 0.0f;
        }

        float m_massScale;
        float m_impulseScale;

        PIP3D_FORCE_INLINE void preStep(ContactManifold &info, float deltaTime,
                                        const SoftConstraint &soft) noexcept
        {
            RigidBody *a = info.bodyA;
            RigidBody *b = info.bodyB;
            if (!a || !b)
                return;
            if (a->isTrigger || b->isTrigger)
                return;

            const bool aImmovable = a->isStatic || a->isKinematic || a->isSleeping;
            const bool bImmovable = b->isStatic || b->isKinematic || b->isSleeping;
            if (aImmovable && bImmovable)
                return;

            const float invMassA = aImmovable ? 0.0f : a->invMass;
            const float invMassB = bImmovable ? 0.0f : b->invMass;
            const Vector3 n = info.normal;

            const float restF = sqrtf(a->restitution * b->restitution);
            const float fricF = sqrtf(a->friction * b->friction);
            const uint16_t material = packMaterial(restF, fricF);

            Vector3 t1, t2;
            buildTangentBasis(n, t1, t2);

            m_massScale = 1.0f;
            m_impulseScale = 0.0f;

            for (int ci = 0; ci < info.contactCount; ++ci)
            {
                Contact &c = info.contacts[ci];
                c.normalMass = 0.0f;
                c.tangentMass1 = 0.0f;
                c.tangentMass2 = 0.0f;
                c.bias = 0.0f;
                c.material = material;

                const Vector3 rA = c.pos - a->position;
                const Vector3 rB = c.pos - b->position;

                c.normalMass = effectiveMass(a, b, rA, rB, n, invMassA, invMassB);
                c.tangentMass1 = effectiveMass(a, b, rA, rB, t1, invMassA, invMassB);
                c.tangentMass2 = effectiveMass(a, b, rA, rB, t2, invMassA, invMassB);

                const Vector3 vA = a->velocity + a->angularVelocity.cross(rA);
                const Vector3 vB = b->velocity + b->angularVelocity.cross(rB);
                const Vector3 rv = vB - vA;
                const float vn = rv.dot(n);

                if (vn < -PhysicsConfig::RESTITUTION_THRESHOLD)
                    c.bias = -restF * vn;
                else
                    c.bias = 0.0f;
            }
        }

        PIP3D_FORCE_INLINE void warmStart(ContactManifold *contacts, int count) noexcept
        {
            for (int ci = 0; ci < count; ++ci)
            {
                ContactManifold &info = contacts[ci];
                RigidBody *a = info.bodyA;
                RigidBody *b = info.bodyB;
                if (!a || !b)
                    continue;
                if (a->isTrigger || b->isTrigger)
                    continue;

                const bool aImmovable = a->isStatic || a->isKinematic || a->isSleeping;
                const bool bImmovable = b->isStatic || b->isKinematic || b->isSleeping;
                if (aImmovable && bImmovable)
                    continue;

                const float invMassA = a->invMass;
                const float invMassB = b->invMass;
                const Vector3 n = info.normal;

                Vector3 t1, t2;
                buildTangentBasis(n, t1, t2);

                for (int j = 0; j < info.contactCount; ++j)
                {
                    Contact &c = info.contacts[j];
                    const Vector3 impulse = n * c.accumulatedImpulse +
                                            t1 * c.tangentImpulse1 +
                                            t2 * c.tangentImpulse2;
                    if (impulse.lengthSquared() <= 1e-12f)
                        continue;

                    const Vector3 rA = c.pos - a->position;
                    const Vector3 rB = c.pos - b->position;

                    if (!aImmovable && invMassA > 0.0f)
                    {
                        a->velocity -= impulse * invMassA;
                        a->angularVelocity -= a->mulWorldInvInertia(rA.cross(impulse));
                    }
                    if (!bImmovable && invMassB > 0.0f)
                    {
                        b->velocity += impulse * invMassB;
                        b->angularVelocity += b->mulWorldInvInertia(rB.cross(impulse));
                    }
                }
            }
        }

        PIP3D_FORCE_INLINE void solve(ContactManifold &info, float deltaTime,
                                      bool useBias) noexcept
        {
            RigidBody *a = info.bodyA;
            RigidBody *b = info.bodyB;
            if (!a || !b)
                return;
            if (a->isTrigger || b->isTrigger)
                return;

            const bool aImmovable = a->isStatic || a->isKinematic || a->isSleeping;
            const bool bImmovable = b->isStatic || b->isKinematic || b->isSleeping;
            if (aImmovable && bImmovable)
                return;
            if (info.contactCount <= 0)
                return;

            const float invMassA = aImmovable ? 0.0f : a->invMass;
            const float invMassB = bImmovable ? 0.0f : b->invMass;
            const float invMassSum = invMassA + invMassB;
            if (invMassSum <= 0.0f)
                return;

            const Vector3 n = info.normal;
            Vector3 t1, t2;
            buildTangentBasis(n, t1, t2);

            const float friction = unpackFriction(info.contacts[0].material);

            for (int ci = 0; ci < info.contactCount; ++ci)
            {
                Contact &ct = info.contacts[ci];
                const Vector3 rA = ct.pos - a->position;
                const Vector3 rB = ct.pos - b->position;

                Vector3 vA = a->velocity + a->angularVelocity.cross(rA);
                Vector3 vB = b->velocity + b->angularVelocity.cross(rB);
                Vector3 rv = vB - vA;
                float vn = rv.dot(n);

                float lambda;
                if (useBias)
                {
                    lambda = -ct.normalMass * (vn + ct.bias);
                }
                else
                {
                    lambda = -ct.normalMass * vn;
                }

                float oldImpulse = ct.accumulatedImpulse;
                float newImpulse = oldImpulse + lambda;
                if (newImpulse < 0.0f)
                    newImpulse = 0.0f;
                float dImpulse = newImpulse - oldImpulse;
                ct.accumulatedImpulse = newImpulse;

                if (dImpulse != 0.0f)
                {
                    const Vector3 impulse = n * dImpulse;
                    if (!aImmovable && invMassA > 0.0f)
                    {
                        a->velocity -= impulse * invMassA;
                        a->angularVelocity -= a->mulWorldInvInertia(rA.cross(impulse));
                    }
                    if (!bImmovable && invMassB > 0.0f)
                    {
                        b->velocity += impulse * invMassB;
                        b->angularVelocity += b->mulWorldInvInertia(rB.cross(impulse));
                    }
                }

                vA = a->velocity + a->angularVelocity.cross(rA);
                vB = b->velocity + b->angularVelocity.cross(rB);
                rv = vB - vA;

                const float vt1 = rv.dot(t1);
                const float dL1 = -vt1 * ct.tangentMass1;
                const float oldT1 = ct.tangentImpulse1;
                ct.tangentImpulse1 = oldT1 + dL1;

                const float vt2 = rv.dot(t2);
                const float dL2 = -vt2 * ct.tangentMass2;
                const float oldT2 = ct.tangentImpulse2;
                ct.tangentImpulse2 = oldT2 + dL2;

                const float maxFriction = friction * ct.accumulatedImpulse;
                if (maxFriction > 0.0f)
                {
                    const float tMagSq = ct.tangentImpulse1 * ct.tangentImpulse1 +
                                         ct.tangentImpulse2 * ct.tangentImpulse2;
                    if (tMagSq > maxFriction * maxFriction)
                    {
                        const float tMag = sqrtf(tMagSq);
                        const float scale = maxFriction / tMag;
                        ct.tangentImpulse1 *= scale;
                        ct.tangentImpulse2 *= scale;
                    }
                }
                else
                {
                    ct.tangentImpulse1 = 0.0f;
                    ct.tangentImpulse2 = 0.0f;
                }

                const float dT1 = ct.tangentImpulse1 - oldT1;
                const float dT2 = ct.tangentImpulse2 - oldT2;
                const Vector3 frictionImpulse = t1 * dT1 + t2 * dT2;

                if (!aImmovable && invMassA > 0.0f)
                {
                    a->velocity -= frictionImpulse * invMassA;
                    a->angularVelocity -= a->mulWorldInvInertia(rA.cross(frictionImpulse));
                }
                if (!bImmovable && invMassB > 0.0f)
                {
                    b->velocity += frictionImpulse * invMassB;
                    b->angularVelocity += b->mulWorldInvInertia(rB.cross(frictionImpulse));
                }
            }
        }

        PIP3D_FORCE_INLINE void positionalCorrection(ContactManifold *contacts, int count) noexcept
        {

            (void)contacts;
            (void)count;
        }
    };
}

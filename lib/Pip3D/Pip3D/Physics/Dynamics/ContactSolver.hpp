#pragma once

#include <vector>

#include "Math/Collision.hpp"
#include "Body.hpp"
#include "Contacts.hpp"
#include "../Types.hpp"

namespace pip3D
{
    class ContactSolver
    {
    public:
        __attribute__((always_inline)) inline static void
        buildTangentBasis(const Vector3 &n, Vector3 &t1, Vector3 &t2) noexcept
        {
            if (fabsf(n.x) <= 0.70710678f)
                t1 = Vector3(1.0f - n.x * n.x, -n.y * n.x, -n.z * n.x);
            else
                t1 = Vector3(-n.x * n.y, 1.0f - n.y * n.y, -n.z * n.y);
            t1.normalize();
            t2 = n.cross(t1);
        }

        __attribute__((always_inline)) inline static float
        effectiveMass(const RigidBody *a, const RigidBody *b,
                      const Vector3 &rA, const Vector3 &rB,
                      const Vector3 &d, float invMassSum) noexcept
        {
            Vector3 rAxd = rA.cross(d);
            Vector3 rBxd = rB.cross(d);
            Vector3 invIA = a->mulWorldInvInertia(rAxd);
            Vector3 invIB = b->mulWorldInvInertia(rBxd);
            float angTerm = invIA.cross(rA).dot(d) + invIB.cross(rB).dot(d);
            float denom = invMassSum + angTerm;
            return denom > 0.0f ? 1.0f / denom : 0.0f;
        }

        PIP3D_FORCE_INLINE void preStep(ContactManifold &info, float deltaTime)
        {
            RigidBody *a = info.bodyA;
            RigidBody *b = info.bodyB;
            if (!a || !b)
                return;
            if (a->isTrigger || b->isTrigger)
                return;

            float invMassA = a->invMass;
            float invMassB = b->invMass;
            float invMassSum = invMassA + invMassB;
            Vector3 n = info.normal;

            Vector3 t1, t2;
            buildTangentBasis(n, t1, t2);

            const float restitution = sqrtf(fmaxf(0.0f, a->restitution) * fmaxf(0.0f, b->restitution));
            const float friction = sqrtf(fmaxf(0.0f, a->friction) * fmaxf(0.0f, b->friction));

            ContactManifold *old = nullptr;
            Vector3 oldNormal(0, 1, 0);
            const size_t prevCount = previousContacts_.size();
            for (size_t i = 0; i < prevCount; ++i)
            {
                ContactManifold &prev = previousContacts_[i];
                if (prev.bodyA == a && prev.bodyB == b)
                {
                    old = &prev;
                    oldNormal = prev.normal;
                    break;
                }
                if (prev.bodyA == b && prev.bodyB == a)
                {
                    old = &prev;
                    oldNormal = prev.normal * -1.0f;
                    break;
                }
            }

            bool used[4] = {false, false, false, false};

            for (int ci = 0; ci < info.contactCount; ++ci)
            {
                Contact &c = info.contacts[ci];
                c.normalMass = 0.0f;
                c.tangentMass1 = 0.0f;
                c.tangentMass2 = 0.0f;
                c.bias = 0.0f;
                c.restitution = restitution;
                c.friction = friction;
                c.tangent1 = t1;
                c.tangent2 = t2;

                bool isPredictive = (c.penetration <= 0.0f);

                if (!isPredictive && old)
                {
                    int oldCount = old->contactCount > 4 ? 4 : old->contactCount;
                    for (int oi = 0; oi < oldCount; ++oi)
                    {
                        if (used[oi])
                            continue;
                        if (old->contacts[oi].featureId == c.featureId)
                        {
                            c.accumulatedImpulse = old->contacts[oi].accumulatedImpulse;

                            float normalDot = oldNormal.dot(n);
                            if (normalDot > 0.9f)
                            {
                                c.tangentImpulse1 = old->contacts[oi].tangentImpulse1;
                                c.tangentImpulse2 = old->contacts[oi].tangentImpulse2;
                            }
                            else
                            {
                                c.tangentImpulse1 = 0.0f;
                                c.tangentImpulse2 = 0.0f;
                            }
                            used[oi] = true;
                            break;
                        }
                    }
                }
                else
                {
                    c.accumulatedImpulse = 0.0f;
                    c.tangentImpulse1 = 0.0f;
                    c.tangentImpulse2 = 0.0f;
                }

                Vector3 rA = c.pos - a->position;
                Vector3 rB = c.pos - b->position;
                c.normalMass = effectiveMass(a, b, rA, rB, n, invMassSum);
                c.tangentMass1 = effectiveMass(a, b, rA, rB, t1, invMassSum);
                c.tangentMass2 = effectiveMass(a, b, rA, rB, t2, invMassSum);

                if (!isPredictive)
                {

                    Vector3 vA = a->velocity + a->angularVelocity.cross(rA);
                    Vector3 vB = b->velocity + b->angularVelocity.cross(rB);
                    Vector3 rv = vB - vA;
                    float vn = rv.dot(n);
                    if (vn < -PhysicsConfig::RESTITUTION_THRESHOLD)
                        c.bias = -restitution * vn;
                    else
                        c.bias = 0.0f;
                }
                else
                {

                    c.bias = 0.0f;
                }
            }
        }

        PIP3D_FORCE_INLINE void warmStart(std::vector<ContactManifold> &contacts)
        {
            const size_t count = contacts.size();
            for (size_t ci = 0; ci < count; ++ci)
            {
                ContactManifold &info = contacts[ci];
                RigidBody *a = info.bodyA;
                RigidBody *b = info.bodyB;
                if (!a || !b)
                    continue;
                if (a->isTrigger || b->isTrigger)
                    continue;

                bool aImmovable = a->isStatic || a->isKinematic || a->isSleeping;
                bool bImmovable = b->isStatic || b->isKinematic || b->isSleeping;
                if (aImmovable && bImmovable)
                    continue;

                const float invMassA = a->invMass;
                const float invMassB = b->invMass;
                const Vector3 n = info.normal;

                for (int j = 0; j < info.contactCount; ++j)
                {
                    Contact &c = info.contacts[j];
                    if (c.penetration <= 0.0f)
                        continue;

                    Vector3 impulse = n * c.accumulatedImpulse + c.tangent1 * c.tangentImpulse1 + c.tangent2 * c.tangentImpulse2;
                    if (impulse.lengthSquared() <= 1e-12f)
                        continue;

                    Vector3 rA = c.pos - a->position;
                    Vector3 rB = c.pos - b->position;

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

        PIP3D_FORCE_INLINE void solve(ContactManifold &info, float deltaTime)
        {
            RigidBody *a = info.bodyA;
            RigidBody *b = info.bodyB;
            if (!a || !b)
                return;
            if (a->isTrigger || b->isTrigger)
                return;

            bool aImmovable = a->isStatic || a->isKinematic || a->isSleeping;
            bool bImmovable = b->isStatic || b->isKinematic || b->isSleeping;
            if (aImmovable && bImmovable)
                return;
            if (info.contactCount <= 0)
                return;

            const float invMassA = a->invMass;
            const float invMassB = b->invMass;
            const float invMassSum = invMassA + invMassB;
            if (invMassSum <= 0.0f)
                return;

            const Vector3 n = info.normal;

            for (int ci = 0; ci < info.contactCount; ++ci)
            {
                Contact &ct = info.contacts[ci];
                const Vector3 rA = ct.pos - a->position;
                const Vector3 rB = ct.pos - b->position;
                const bool isPredictive = (ct.penetration <= 0.0f);

                Vector3 vA = a->velocity + a->angularVelocity.cross(rA);
                Vector3 vB = b->velocity + b->angularVelocity.cross(rB);
                Vector3 rv = vB - vA;
                float vn = rv.dot(n);

                float lambda = -(vn + ct.bias) * ct.normalMass;

                float oldImpulse = ct.accumulatedImpulse;
                float newImpulse = oldImpulse + lambda;
                if (newImpulse < 0.0f)
                    newImpulse = 0.0f;
                float dImpulse = newImpulse - oldImpulse;
                ct.accumulatedImpulse = newImpulse;

                if (dImpulse != 0.0f)
                {
                    Vector3 impulse = n * dImpulse;
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

                if (!isPredictive && ct.penetration > PhysicsConfig::POSITION_SLOP && deltaTime > 0.0f && ct.normalMass > 0.0f)
                {
                    float pen = ct.penetration - PhysicsConfig::POSITION_SLOP;

                    float correction = pen * PhysicsConfig::POSITION_PERCENT;
                    const float kMaxStepCorrection = 0.005f;
                    if (correction > kMaxStepCorrection)
                        correction = kMaxStepCorrection;

                    float pseudoBias = correction / deltaTime;

                    Vector3 pvA = a->posCorrectionVelocity + a->posCorrectionAngular.cross(rA);
                    Vector3 pvB = b->posCorrectionVelocity + b->posCorrectionAngular.cross(rB);
                    float pvn = (pvB - pvA).dot(n);

                    float plambda = (pseudoBias - pvn) * ct.normalMass;
                    if (plambda < 0.0f)
                        plambda = 0.0f;

                    Vector3 pImpulse = n * plambda;
                    if (!aImmovable && invMassA > 0.0f)
                    {
                        a->posCorrectionVelocity -= pImpulse * invMassA;
                        a->posCorrectionAngular -= a->mulWorldInvInertia(rA.cross(pImpulse));
                    }
                    if (!bImmovable && invMassB > 0.0f)
                    {
                        b->posCorrectionVelocity += pImpulse * invMassB;
                        b->posCorrectionAngular += b->mulWorldInvInertia(rB.cross(pImpulse));
                    }
                }

                if (!isPredictive)
                {

                    vA = a->velocity + a->angularVelocity.cross(rA);
                    vB = b->velocity + b->angularVelocity.cross(rB);
                    rv = vB - vA;

                    float vt1 = rv.dot(ct.tangent1);
                    float dL1 = -vt1 * ct.tangentMass1;
                    float oldT1 = ct.tangentImpulse1;
                    ct.tangentImpulse1 = oldT1 + dL1;

                    float vt2 = rv.dot(ct.tangent2);
                    float dL2 = -vt2 * ct.tangentMass2;
                    float oldT2 = ct.tangentImpulse2;
                    ct.tangentImpulse2 = oldT2 + dL2;

                    float maxFriction = ct.friction * ct.accumulatedImpulse;
                    if (maxFriction > 0.0f)
                    {
                        float tMagSq = ct.tangentImpulse1 * ct.tangentImpulse1 + ct.tangentImpulse2 * ct.tangentImpulse2;
                        if (tMagSq > maxFriction * maxFriction)
                        {
                            float tMag = sqrtf(tMagSq);
                            float scale = maxFriction / tMag;
                            ct.tangentImpulse1 *= scale;
                            ct.tangentImpulse2 *= scale;
                        }
                    }
                    else
                    {
                        ct.tangentImpulse1 = 0.0f;
                        ct.tangentImpulse2 = 0.0f;
                    }

                    float dT1 = ct.tangentImpulse1 - oldT1;
                    float dT2 = ct.tangentImpulse2 - oldT2;
                    Vector3 frictionImpulse = ct.tangent1 * dT1 + ct.tangent2 * dT2;

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
        }

        PIP3D_FORCE_INLINE void integratePseudoVel(std::vector<RigidBody *> &bodies,
                                                   float deltaTime)
        {
            const size_t count = bodies.size();
            for (size_t i = 0; i < count; ++i)
            {
                RigidBody *b = bodies[i];
                if (b)
                    b->integratePseudoVelocity(deltaTime);
            }
        }

        PIP3D_FORCE_INLINE void positionalCorrection(std::vector<ContactManifold> &contacts)
        {
            const float emergencyThreshold = 4.0f * PhysicsConfig::POSITION_SLOP;
            const float kMaxResolvePen = 0.05f;

            const size_t count = contacts.size();
            for (size_t i = 0; i < count; ++i)
            {
                ContactManifold &info = contacts[i];
                RigidBody *a = info.bodyA;
                RigidBody *b = info.bodyB;
                if (!a || !b)
                    continue;
                if (a->isTrigger || b->isTrigger)
                    continue;

                bool aImmovable = a->isStatic || a->isKinematic;
                bool bImmovable = b->isStatic || b->isKinematic;
                if (aImmovable && bImmovable)
                    continue;

                float invMassA = a->invMass;
                float invMassB = b->invMass;
                float invMassSum = invMassA + invMassB;
                if (invMassSum <= 0.0f)
                    continue;

                float maxPenetration = 0.0f;
                for (int ci = 0; ci < info.contactCount; ++ci)
                {
                    float p = info.contacts[ci].penetration;
                    if (p > maxPenetration)
                        maxPenetration = p;
                }
                if (maxPenetration < emergencyThreshold)
                    continue;
                if (maxPenetration > kMaxResolvePen)
                    maxPenetration = kMaxResolvePen;

                float overage = maxPenetration - emergencyThreshold;
                float correctionMag = overage * 0.1f;
                if (correctionMag <= 0.0f)
                    continue;

                Vector3 correction = info.normal * correctionMag;
                float aFactor = aImmovable ? 0.0f : invMassA / invMassSum;
                float bFactor = bImmovable ? 0.0f : invMassB / invMassSum;
                if (!aImmovable)
                {
                    a->position -= correction * aFactor;
                    a->updateBoundsFromTransform();
                }
                if (!bImmovable)
                {
                    b->position += correction * bFactor;
                    b->updateBoundsFromTransform();
                }
            }
        }

        PIP3D_FORCE_INLINE void commitFrame(const std::vector<ContactManifold> &contacts)
        {

            if (contacts.size() > 256)
                previousContacts_.assign(contacts.end() - 256, contacts.end());
            else
                previousContacts_ = contacts;
        }

        PIP3D_FORCE_INLINE void clear() { previousContacts_.clear(); }

    private:
        std::vector<ContactManifold> previousContacts_;
    };
}

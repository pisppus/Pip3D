#pragma once

#include <math.h>

#include "Physics/RigidBody/Body.hpp"
#include "Physics/Types.hpp"

namespace pip3D
{

    struct Constraint
    {
        RigidBody *a;
        RigidBody *b;
        bool enabled;

        Constraint(RigidBody *a_, RigidBody *b_)
            : a(a_), b(b_), enabled(true) {}

        virtual ~Constraint() {}

        virtual void preStep(float deltaTime) = 0;
        virtual void solve(float deltaTime) = 0;
    };

    __attribute__((always_inline)) inline void
    computeSoftConstraint(float frequencyHz, float dampingRatio, float dt,
                          float &outBeta, float &outSoft) noexcept
    {

        if (frequencyHz <= 0.0f)
            frequencyHz = 1.0f;
        float omega = 2.0f * kPi * frequencyHz;
        float omegaDt = omega * dt;
        float dampOmegaDt = 4.0f * dampingRatio * omegaDt;
        float denom = 1.0f + dampOmegaDt + omegaDt * omegaDt;
        outBeta = dampOmegaDt / denom;
        outSoft = (omegaDt * omegaDt) / denom;
    }

    struct DistanceConstraint : public Constraint
    {
        Vector3 localAnchorA;
        Vector3 localAnchorB;
        float restLength;

        float frequencyHz;
        float dampingRatio;

        float effectiveMass;
        float bias;
        float softness;
        float gamma;
        Vector3 rA;
        Vector3 rB;
        Vector3 n;
        float accumulatedImpulse;

        DistanceConstraint(RigidBody *a_,
                           RigidBody *b_,
                           const Vector3 &localA,
                           const Vector3 &localB,
                           float restLength_)
            : Constraint(a_, b_),
              localAnchorA(localA),
              localAnchorB(localB),
              restLength(restLength_),
              frequencyHz(10.0f),
              dampingRatio(0.7f),
              effectiveMass(0.0f),
              bias(0.0f),
              softness(0.0f),
              gamma(0.0f),
              rA(0.0f, 0.0f, 0.0f),
              rB(0.0f, 0.0f, 0.0f),
              n(0.0f, 1.0f, 0.0f),
              accumulatedImpulse(0.0f)
        {
        }

        virtual void preStep(float deltaTime) override
        {
            if (!enabled)
                return;
            RigidBody *aBody = a;
            RigidBody *bBody = b;
            if (!aBody || !bBody)
                return;

            if (aBody->isSleeping != bBody->isSleeping)
            {
                if (aBody->isSleeping)
                    aBody->wakeUp();
                if (bBody->isSleeping)
                    bBody->wakeUp();
            }

            const bool aImmovable = aBody->isStatic || aBody->isKinematic || aBody->isSleeping;
            const bool bImmovable = bBody->isStatic || bBody->isKinematic || bBody->isSleeping;
            const float invMassA = aImmovable ? 0.0f : aBody->invMass;
            const float invMassB = bImmovable ? 0.0f : bBody->invMass;
            float invMassSum = invMassA + invMassB;
            if (invMassSum <= 0.0f)
            {
                effectiveMass = 0.0f;
                bias = 0.0f;
                gamma = 0.0f;
                return;
            }

            rA = aBody->orientation.rotate(localAnchorA);
            rB = bBody->orientation.rotate(localAnchorB);

            Vector3 delta = (bBody->position + rB) - (aBody->position + rA);
            float distSq = delta.lengthSquared();
            float C = 0.0f;

            if (distSq > 1e-8f)
            {
                float dist = sqrtf(distSq);
                float invLen = FastMath::fastInvSqrt(distSq);
                n = delta * invLen;
                C = dist - restLength;
            }
            else
            {
                n = Vector3(0.0f, 1.0f, 0.0f);
                C = -restLength;
            }

            Vector3 rAxn = rA.cross(n);
            Vector3 rBxn = rB.cross(n);
            Vector3 invIA = invMassA > 0.0f ? aBody->mulWorldInvInertia(rAxn) : Vector3(0, 0, 0);
            Vector3 invIB = invMassB > 0.0f ? bBody->mulWorldInvInertia(rBxn) : Vector3(0, 0, 0);
            float angularTerm = invIA.cross(rA).dot(n) + invIB.cross(rB).dot(n);
            float denom = invMassSum + angularTerm;

            if (denom <= 0.0f || deltaTime <= 0.0f)
            {
                effectiveMass = 0.0f;
                bias = 0.0f;
                gamma = 0.0f;
                return;
            }

            const float rigidMass = 1.0f / denom;
            if (frequencyHz > 0.0f)
            {
                const float omega = 2.0f * kPi * frequencyHz;
                const float damping = 2.0f * rigidMass * dampingRatio * omega;
                const float stiffness = rigidMass * omega * omega;
                const float gammaDenom = deltaTime * (damping + deltaTime * stiffness);
                gamma = gammaDenom > 1e-12f ? 1.0f / gammaDenom : 0.0f;
                const float beta = deltaTime * stiffness * gamma;

                bias = beta * C;
            }
            else
            {
                gamma = 0.0f;
                bias = 0.20f * C / deltaTime;
            }
            effectiveMass = 1.0f / (denom + gamma);

            if (accumulatedImpulse != 0.0f)
            {
                Vector3 impulse = n * accumulatedImpulse;
                if (!aImmovable && invMassA > 0.0f)
                {
                    aBody->velocity -= impulse * invMassA;
                    aBody->angularVelocity -= aBody->mulWorldInvInertia(rA.cross(impulse));
                }
                if (!bImmovable && invMassB > 0.0f)
                {
                    bBody->velocity += impulse * invMassB;
                    bBody->angularVelocity += bBody->mulWorldInvInertia(rB.cross(impulse));
                }
            }
        }

        virtual void solve(float deltaTime) override
        {
            if (!enabled)
                return;
            RigidBody *aBody = a;
            RigidBody *bBody = b;
            if (!aBody || !bBody)
                return;
            if (effectiveMass <= 0.0f)
                return;

            bool aImmovable = aBody->isStatic || aBody->isKinematic || aBody->isSleeping;
            bool bImmovable = bBody->isStatic || bBody->isKinematic || bBody->isSleeping;
            if (aImmovable && bImmovable)
                return;

            float invMassA = aImmovable ? 0.0f : aBody->invMass;
            float invMassB = bImmovable ? 0.0f : bBody->invMass;

            Vector3 vA = aBody->velocity + aBody->angularVelocity.cross(rA);
            Vector3 vB = bBody->velocity + bBody->angularVelocity.cross(rB);
            Vector3 rv = vB - vA;
            float vn = rv.dot(n);

            float lambda = -(vn + bias + gamma * accumulatedImpulse) * effectiveMass;

            float oldImpulse = accumulatedImpulse;
            accumulatedImpulse += lambda;

            float dImpulse = accumulatedImpulse - oldImpulse;
            Vector3 impulse = n * dImpulse;

            if (!aImmovable && invMassA > 0.0f)
            {
                aBody->velocity -= impulse * invMassA;
                aBody->angularVelocity -= aBody->mulWorldInvInertia(rA.cross(impulse));
            }
            if (!bImmovable && invMassB > 0.0f)
            {
                bBody->velocity += impulse * invMassB;
                bBody->angularVelocity += bBody->mulWorldInvInertia(rB.cross(impulse));
            }
        }
    };

    struct PointConstraint : public DistanceConstraint
    {
        PointConstraint(RigidBody *a_,
                        RigidBody *b_,
                        const Vector3 &localA,
                        const Vector3 &localB)
            : DistanceConstraint(a_, b_, localA, localB, 0.0f)
        {

            frequencyHz = 30.0f;
            dampingRatio = 1.0f;
        }
    };
}

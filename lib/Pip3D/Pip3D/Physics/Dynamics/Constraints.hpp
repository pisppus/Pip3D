#pragma once

#include <math.h>

#include "Body.hpp"
#include "../Types.hpp"

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

            float invMassA = aBody->invMass;
            float invMassB = bBody->invMass;
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
            Vector3 invIA = aBody->mulWorldInvInertia(rAxn);
            Vector3 invIB = bBody->mulWorldInvInertia(rBxn);
            float angularTerm = invIA.cross(rA).dot(n) + invIB.cross(rB).dot(n);
            float denom = invMassSum + angularTerm;

            float beta, soft;
            computeSoftConstraint(frequencyHz, dampingRatio, deltaTime, beta, soft);

            float softOverDt = (deltaTime > 0.0f) ? soft / deltaTime : 0.0f;
            float kPlusSoft = denom + softOverDt;
            effectiveMass = (kPlusSoft > 0.0f) ? 1.0f / kPlusSoft : 0.0f;
            gamma = (kPlusSoft > 0.0f) ? softOverDt * effectiveMass : 0.0f;

            bias = (deltaTime > 0.0f) ? -beta * C / deltaTime : 0.0f;

            if (accumulatedImpulse != 0.0f)
            {
                Vector3 impulse = n * accumulatedImpulse;
                bool aImmovable = aBody->isStatic || aBody->isKinematic;
                bool bImmovable = bBody->isStatic || bBody->isKinematic;
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

            bool aImmovable = aBody->isStatic || aBody->isKinematic;
            bool bImmovable = bBody->isStatic || bBody->isKinematic;
            if (aImmovable && bImmovable)
                return;

            float invMassA = aBody->invMass;
            float invMassB = bBody->invMass;

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

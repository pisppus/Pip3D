#pragma once

#include "Body.hpp"

namespace pip3D
{
        __attribute__((always_inline)) inline void RigidBody::beginStep()
        {
            previousPosition = position;
            previousOrientation = orientation;
            posCorrectionVelocity = Vector3(0, 0, 0);
            posCorrectionAngular = Vector3(0, 0, 0);
            acceleration = Vector3(0, 0, 0);

            if (!isStatic && !isSleeping)
                updateWorldInvInertia();
        }

        __attribute__((always_inline)) inline void RigidBody::update(float deltaTime)
        {
            if (isStatic || isSleeping || isKinematic)
                return;

            velocity += acceleration * deltaTime;

            float linFactor = 1.0f - linearDamping * deltaTime;
            float angFactor = 1.0f - angularDamping * deltaTime;
            if (linFactor < 0.0f)
                linFactor = 0.0f;
            if (angFactor < 0.0f)
                angFactor = 0.0f;
            velocity *= linFactor;
            angularVelocity *= angFactor;

            float vLenSq = velocity.lengthSquared();
            if (vLenSq < 1e-6f)
            {
                velocity = Vector3(0, 0, 0);
                vLenSq = 0.0f;
            }
            else if (vLenSq > PhysicsConfig::MAX_LINEAR_VELOCITY * PhysicsConfig::MAX_LINEAR_VELOCITY)
            {
                velocity *= PhysicsConfig::MAX_LINEAR_VELOCITY * FastMath::fastInvSqrt(vLenSq);
            }

            float angLenSq = angularVelocity.lengthSquared();
            if (angLenSq < 1e-6f)
            {
                angularVelocity = Vector3(0, 0, 0);
                angLenSq = 0.0f;
            }
            else if (angLenSq > PhysicsConfig::MAX_ANGULAR_VELOCITY * PhysicsConfig::MAX_ANGULAR_VELOCITY)
            {
                angularVelocity *= PhysicsConfig::MAX_ANGULAR_VELOCITY * FastMath::fastInvSqrt(angLenSq);
                angLenSq = angularVelocity.lengthSquared();
            }

            position += velocity * deltaTime;

            if (angLenSq > 1e-8f)
            {
                float halfDt = 0.5f * deltaTime;
                Quaternion omega(
                    angularVelocity.x * halfDt,
                    angularVelocity.y * halfDt,
                    angularVelocity.z * halfDt,
                    0.0f);
                Quaternion deltaQ = omega * orientation;
                orientation.x += deltaQ.x;
                orientation.y += deltaQ.y;
                orientation.z += deltaQ.z;
                orientation.w += deltaQ.w;
                orientation.normalize();
            }

            updateBoundsFromTransform();
        }

        __attribute__((always_inline)) inline void RigidBody::integratePseudoVelocity(float deltaTime)
        {
            if (isStatic || isSleeping || isKinematic)
                return;

            position += posCorrectionVelocity * deltaTime;

            float angLenSq = posCorrectionAngular.lengthSquared();
            if (angLenSq > 1e-10f)
            {
                float halfDt = 0.5f * deltaTime;
                Quaternion omega(
                    posCorrectionAngular.x * halfDt,
                    posCorrectionAngular.y * halfDt,
                    posCorrectionAngular.z * halfDt,
                    0.0f);
                Quaternion deltaQ = omega * orientation;
                orientation.x += deltaQ.x;
                orientation.y += deltaQ.y;
                orientation.z += deltaQ.z;
                orientation.w += deltaQ.w;
                orientation.normalize();
            }

            updateBoundsFromTransform();
        }

        __attribute__((always_inline)) inline void RigidBody::interpolateTransforms(float alpha)
        {
            if (alpha < 0.0f)
                alpha = 0.0f;
            if (alpha > 1.0f)
                alpha = 1.0f;

            renderPosition.x = previousPosition.x + (position.x - previousPosition.x) * alpha;
            renderPosition.y = previousPosition.y + (position.y - previousPosition.y) * alpha;
            renderPosition.z = previousPosition.z + (position.z - previousPosition.z) * alpha;

            float qx = previousOrientation.x + (orientation.x - previousOrientation.x) * alpha;
            float qy = previousOrientation.y + (orientation.y - previousOrientation.y) * alpha;
            float qz = previousOrientation.z + (orientation.z - previousOrientation.z) * alpha;
            float qw = previousOrientation.w + (orientation.w - previousOrientation.w) * alpha;

            float dot = previousOrientation.x * orientation.x + previousOrientation.y * orientation.y + previousOrientation.z * orientation.z + previousOrientation.w * orientation.w;
            if (dot < 0.0f)
            {

                qx = previousOrientation.x + (-orientation.x - previousOrientation.x) * alpha;
                qy = previousOrientation.y + (-orientation.y - previousOrientation.y) * alpha;
                qz = previousOrientation.z + (-orientation.z - previousOrientation.z) * alpha;
                qw = previousOrientation.w + (-orientation.w - previousOrientation.w) * alpha;
            }

            renderOrientation.x = qx;
            renderOrientation.y = qy;
            renderOrientation.z = qz;
            renderOrientation.w = qw;
            renderOrientation.normalize();
        }

}

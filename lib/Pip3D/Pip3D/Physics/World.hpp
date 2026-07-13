#pragma once

#include <atomic>
#include <vector>

#include "Math/Collision.hpp"
#include "Core/Jobs.hpp"

#include "Types.hpp"
#include "Dynamics/Body.hpp"
#include "Dynamics/Contacts.hpp"
#include "Dynamics/Constraints.hpp"
#include "Dynamics/Buoyancy.hpp"
#include "Collision/Narrowphase.hpp"
#include "Dynamics/ContactSolver.hpp"
#include "Collision/Raycast.hpp"

namespace pip3D
{
    class Renderer;

    class PhysicsWorld
    {
    private:
        std::vector<RigidBody *> bodies;
        std::vector<Constraint *> constraints;
        std::vector<BuoyancyZone> waterZones;

        std::vector<ContactManifold> contactConstraints;
        ContactSolver solver;

        Vector3 gravity;
        bool asyncEnabled;

        std::atomic<bool> stepInProgress;
        float pendingDelta;

        float fixedTimeStep;
        float accumulator;
        float currentDeltaTime;

        static void stepJobFunc(void *userData);
        void runStepJob();
        void stepInternal(float deltaTime);

        void maybeWake(ContactManifold &info);

        void updateSleepAndSettle(float deltaTime);

        void applyRestingVelocityZeroing();

        void preStepJoints(float deltaTime);
        void solveJoints(float deltaTime);

    public:
        PhysicsWorld();

        bool addBody(RigidBody *body);
        void removeBody(RigidBody *body);
        bool addConstraint(Constraint *c);
        void removeConstraint(Constraint *c);
        void addBuoyancyZone(const BuoyancyZone &zone);

        void setGravity(const Vector3 &g) { gravity = g; }
        const Vector3 &getGravity() const { return gravity; }

        void setAsyncEnabled(bool enabled) { asyncEnabled = enabled; }
        bool isAsyncEnabled() const;
        bool isStepInProgress() const { return stepInProgress; }

        void setFixedTimeStep(float dt) { fixedTimeStep = dt; }
        float getFixedTimeStep() const { return fixedTimeStep; }

        void updateFixed(float frameDelta);
        void stepAsync(float deltaTime);
        void finishStep();

        float getInterpolationAlpha() const;
        void interpolateTransforms(float alpha);

        bool raycast(const Ray &ray, RaycastHit &outHit,
                     float maxDistance = FLT_MAX);

        ContactManifold detectCollision(RigidBody *a, RigidBody *b)
        {
            return pip3D::detectCollision(a, b, currentDeltaTime);
        }

        void Gizmos(Renderer &renderer);
    };
}

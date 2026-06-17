#pragma once

#include "Math/Collision.hpp"
#include "Core/Jobs.hpp"
#include "Body.hpp"
#include "Contacts.hpp"
#include "Constraints.hpp"
#include "Buoyancy.hpp"
#include <vector>
#include <atomic>

namespace pip3D
{
    class Renderer;

    class PhysicsWorld
    {
    private:
        static constexpr int SOLVER_ITERATIONS = 8;
        static constexpr int MAX_SUBSTEPS = 3;
        std::vector<RigidBody *> bodies;
        std::vector<Constraint *> constraints;
        Vector3 gravity;
        bool asyncEnabled;

        std::atomic<bool> stepInProgress;

        float pendingDelta;
        float fixedTimeStep;
        float accumulator;
        float currentDeltaTime;
        std::vector<CollisionInfo> contactConstraints;
        std::vector<CollisionInfo> previousContactConstraints;
        std::vector<BuoyancyZone> waterZones;

        static void stepJobFunc(void *userData);
        void runStepJob();
        void stepInternal(float deltaTime);
        void preStepConstraint(CollisionInfo &info, float deltaTime);
        void warmStartConstraints();
        void positionalCorrection();
        void resolveCollision(CollisionInfo &info);
        void preStepJoints(float deltaTime);
        void solveJoints(float deltaTime);

    public:
        PhysicsWorld();

        bool addBody(RigidBody *body);
        bool addConstraint(Constraint *c);
        void removeConstraint(Constraint *c);
        void addBuoyancyZone(const BuoyancyZone &zone);
        void removeBody(RigidBody *body);

        void setGravity(const Vector3 &g) { gravity = g; }
        const Vector3 &getGravity() const { return gravity; }

        void setAsyncEnabled(bool enabled) { asyncEnabled = enabled; }
        bool isAsyncEnabled() const;
        bool isStepInProgress() const { return stepInProgress; }

        void setFixedTimeStep(float dt) { fixedTimeStep = dt; }
        float getFixedTimeStep() const { return fixedTimeStep; }

        void updateFixed(float frameDelta);
        void stepAsync(float deltaTime);
        void Gizmos(Renderer &renderer);

        bool raycast(const Ray &ray, RaycastHit &outHit, float maxDistance = FLT_MAX);
        CollisionInfo detectCollision(RigidBody *a, RigidBody *b);
    };
}
#include "World.hpp"

#include <algorithm>
#include <cmath>
#include <cfloat>

#include "Debug/Logging.hpp"
#include "Collision/ContinuousCollision.hpp"
#include "Collision/Narrowphase.hpp"
#include "Dynamics/Buoyancy.hpp"
namespace pip3D
{

    PhysicsWorld::PhysicsWorld()
        : gravity(0, -9.81f, 0),
          asyncEnabled(true),
          stepInProgress(false),
          pendingDelta(0.0f),
          fixedTimeStep(PhysicsConfig::DEFAULT_FIXED_TIMESTEP),
          accumulator(0.0f),
          currentDeltaTime(0.0f)
    {
    }
    bool PhysicsWorld::addBody(RigidBody *body)
    {
        if (!body)
        {
            LOGE(::pip3D::Debug::LOG_MODULE_PHYSICS,
                 "PhysicsWorld::addBody called with null body");
            return false;
        }
        bodies.push_back(body);
        LOGI(::pip3D::Debug::LOG_MODULE_PHYSICS,
             "PhysicsWorld::addBody: bodyCount=%u",
             static_cast<unsigned int>(bodies.size()));
        return true;
    }

    void PhysicsWorld::removeBody(RigidBody *body)
    {
        for (size_t i = 0; i < bodies.size(); ++i)
        {
            if (bodies[i] == body)
            {
                bodies[i] = bodies.back();
                bodies.pop_back();
                break;
            }
        }
    }

    bool PhysicsWorld::addConstraint(Constraint *c)
    {
        if (!c)
        {
            LOGE(::pip3D::Debug::LOG_MODULE_PHYSICS,
                 "PhysicsWorld::addConstraint called with null constraint");
            return false;
        }
        constraints.push_back(c);
        return true;
    }

    void PhysicsWorld::removeConstraint(Constraint *c)
    {
        for (size_t i = 0; i < constraints.size(); ++i)
        {
            if (constraints[i] == c)
            {
                constraints[i] = constraints.back();
                constraints.pop_back();
                break;
            }
        }
    }

    void PhysicsWorld::addBuoyancyZone(const BuoyancyZone &zone)
    {
        waterZones.push_back(zone);
    }

    bool PhysicsWorld::isAsyncEnabled() const
    {
        return asyncEnabled && JobSystem::isEnabled();
    }

    void PhysicsWorld::stepInternal(float deltaTime)
    {
        currentDeltaTime = deltaTime;
        const size_t bodyCount = bodies.size();
        const float gravityMag = gravity.length();

        for (size_t i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies[i];
            b->beginStep();

            if (!b->isStatic && !b->isKinematic && !b->isSleeping && b->mass > 0.0f)
                b->acceleration += gravity * b->gravityScale;
        }

        if (!waterZones.empty())
        {
            float effectiveGravity = (gravityMag > 0.0f) ? gravityMag : 9.81f;
            applyBuoyancy(bodies.data(), bodyCount,
                          waterZones.data(), waterZones.size(),
                          effectiveGravity, deltaTime);
        }

        for (size_t i = 0; i < bodyCount; ++i)
            bodies[i]->update(deltaTime);

        contactConstraints.clear();

        for (size_t i = 0; i < bodyCount; ++i)
        {
            RigidBody *a = bodies[i];
            for (size_t j = i + 1; j < bodyCount; ++j)
            {
                RigidBody *b = bodies[j];

                bool aImmobile = (a->isStatic || a->isSleeping || a->isKinematic);
                bool bImmobile = (b->isStatic || b->isSleeping || b->isKinematic);
                if (aImmobile && bImmobile)
                    continue;

                ContactManifold info = pip3D::detectCollision(a, b, currentDeltaTime);

                if (!info.hasCollision || info.contactCount <= 0)
                    info = pip3D::predictContacts(a, b, currentDeltaTime);

                if (!info.hasCollision || info.contactCount <= 0)
                    continue;

                maybeWake(info);

                solver.preStep(info, deltaTime);
                contactConstraints.push_back(info);
            }
        }

        preStepJoints(deltaTime);
        solver.warmStart(contactConstraints);

        for (int iter = 0; iter < PhysicsConfig::SOLVER_ITERATIONS; ++iter)
        {
            const size_t contactCount = contactConstraints.size();
            for (size_t c = 0; c < contactCount; ++c)
                solver.solve(contactConstraints[c], deltaTime);

            solveJoints(deltaTime);
        }

        solver.integratePseudoVel(bodies, deltaTime);

        solver.positionalCorrection(contactConstraints);

        applyRestingVelocityZeroing();

        solver.commitFrame(contactConstraints);

        updateSleepAndSettle(deltaTime);
    }

    float PhysicsWorld::getInterpolationAlpha() const
    {
        if (fixedTimeStep <= 0.0f)
            return 0.0f;
        float a = accumulator / fixedTimeStep;
        if (a < 0.0f)
            a = 0.0f;
        if (a > 1.0f)
            a = 1.0f;
        return a;
    }

    void PhysicsWorld::interpolateTransforms(float alpha)
    {
        const size_t bodyCount = bodies.size();
        for (size_t i = 0; i < bodyCount; ++i)
            bodies[i]->interpolateTransforms(alpha);
    }

    void PhysicsWorld::preStepJoints(float deltaTime)
    {
        const size_t count = constraints.size();
        for (size_t i = 0; i < count; ++i)
        {
            Constraint *c = constraints[i];
            if (c)
                c->preStep(deltaTime);
        }
    }

    void PhysicsWorld::solveJoints(float deltaTime)
    {
        const size_t count = constraints.size();
        for (size_t i = 0; i < count; ++i)
        {
            Constraint *c = constraints[i];
            if (c)
                c->solve(deltaTime);
        }
    }

}

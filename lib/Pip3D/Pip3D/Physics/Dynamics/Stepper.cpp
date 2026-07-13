#include "../World.hpp"

#include <algorithm>

namespace pip3D
{
    void PhysicsWorld::stepAsync(float deltaTime)
    {
        if (!isAsyncEnabled())
        {
            stepInternal(deltaTime);
            return;
        }

        finishStep();

        pendingDelta = deltaTime;

        if (JobSystem::submit(&PhysicsWorld::stepJobFunc, this))
            stepInProgress = true;
        else
        {
            LOGW(::pip3D::Debug::LOG_MODULE_PHYSICS,
                 "PhysicsWorld::stepAsync: JobSystem::submit failed, "
                 "running synchronously");
            runStepJob();
        }
    }

    void PhysicsWorld::finishStep()
    {
        if (!isAsyncEnabled())
            return;
        while (stepInProgress.load(std::memory_order_acquire))
        {
        }
    }

    void PhysicsWorld::stepJobFunc(void *userData)
    {
        PhysicsWorld *self = static_cast<PhysicsWorld *>(userData);
        if (self)
            self->runStepJob();
    }

    void PhysicsWorld::runStepJob()
    {
        float dt = pendingDelta;
        pendingDelta = 0.0f;

        if (dt <= 0.0f)
        {
            stepInProgress = false;
            return;
        }

        float baseStep = fixedTimeStep > 0.0f ? fixedTimeStep : dt;
        float remaining = dt;
        int steps = 0;

        while (remaining > 0.0f && steps < PhysicsConfig::MAX_SUBSTEPS)
        {
            float curDt = (fixedTimeStep > 0.0f && remaining > baseStep)
                              ? baseStep
                              : remaining;
            stepInternal(curDt);
            remaining -= curDt;
            ++steps;
        }

        stepInProgress = false;
    }

    void PhysicsWorld::updateFixed(float frameDelta)
    {
        float dt = fixedTimeStep;
        if (dt <= 0.0f)
        {
            stepInternal(frameDelta);
            return;
        }

        accumulator += frameDelta;

        float maxAccum = dt * static_cast<float>(PhysicsConfig::MAX_SUBSTEPS);
        if (accumulator > maxAccum)
            accumulator = maxAccum;

        if (isAsyncEnabled())
        {
            if (!stepInProgress && accumulator >= dt)
            {
                stepAsync(dt);
                accumulator -= dt;
            }
        }
        else
        {
            int steps = 0;
            while (accumulator >= dt && steps < PhysicsConfig::MAX_SUBSTEPS)
            {
                stepInternal(dt);
                accumulator -= dt;
                ++steps;
            }
        }
    }

}

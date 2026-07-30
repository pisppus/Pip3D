#include "World.hpp"

#include <cmath>
#include <cfloat>

#include "Debug/Logging.hpp"
#include "Debug/Gizmos.hpp"
#include "Core/Color.hpp"
#include "Collision/Sweep.hpp"
#include "Collision/Narrowphase.hpp"
#include "RigidBody/Buoyancy.hpp"

namespace pip3D
{

    PhysicsWorld::PhysicsWorld()
        : bodyCount_(0),
          constraintCount_(0),
          waterZoneCount_(0),
          contactCount_(0),
          gravity(0, -9.81f, 0),
          gravityMag_(9.81f),
          fixedTimeStep(PhysicsConfig::DEFAULT_FIXED_TIMESTEP),
          accumulator(0.0f)
    {
        for (int i = 0; i < kMaxBodies; ++i)
        {
            sleepSupported_[i] = false;
            sleepDeepPen_[i] = false;
            sleepGravityDriven_[i] = false;
        }
        for (int i = 0; i < kMaxContacts; ++i)
            activeContacts_[i] = nullptr;

#if PIP3D_TARGET_ESP32
        spawnPhysicsTask();
#endif
    }

    PhysicsWorld::~PhysicsWorld()
    {
#if PIP3D_TARGET_ESP32
        if (physicsTask_)
        {
            physicsStop_ = true;
            __sync_synchronize();
            xTaskNotifyGive(physicsTask_);
            vTaskDelay(pdMS_TO_TICKS(10));
            vTaskDelete(physicsTask_);
            physicsTask_ = nullptr;
        }
#endif
    }

#if PIP3D_TARGET_ESP32
    void PhysicsWorld::spawnPhysicsTask()
    {
        const BaseType_t res = xTaskCreatePinnedToCore(
            &PhysicsWorld::physicsTaskEntry,
            "Pip3DPhys",
            8192,
            this,
            5,
            &physicsTask_,
            0);

        if (res != pdPASS)
        {
            physicsTask_ = nullptr;
            LOGE(::pip3D::Debug::LOG_MODULE_PHYSICS,
                 "PhysicsWorld: xTaskCreatePinnedToCore failed on Core 0");
        }
    }

    void PhysicsWorld::physicsTaskEntry(void *userData)
    {
        PhysicsWorld *self = static_cast<PhysicsWorld *>(userData);

        for (;;)
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            if (self->physicsStop_)
                vTaskDelete(nullptr);

            int stepsThisWake = 0;
            const int kMaxStepsPerWake = PhysicsConfig::MAX_SUBSTEPS;

            while (stepsThisWake < kMaxStepsPerWake)
            {
                __sync_synchronize();
                const float pending = self->pendingFrameDelta_;
                if (pending < self->fixedTimeStep)
                    break;

                self->processPendingCommands();
                self->applyKinematicTargets();
                self->runStepJob(self->fixedTimeStep);

                self->pendingFrameDelta_ = pending - self->fixedTimeStep;
                __sync_synchronize();

                self->snapshotRenderTransforms();

                ++stepsThisWake;
                taskYIELD();
            }
        }
    }
#endif

    void PhysicsWorld::processPendingCommands()
    {
        const int n = bodyCount_;
        for (int i = 0; i < n; ++i)
        {
            RigidBody *b = bodies_[i];
            if (!b)
                continue;

            if (b->pendingGrabFlag)
            {
                b->pendingGrabFlag = false;
                __sync_synchronize();
                if (!b->isKinematic)
                    b->setKinematic(true);
            }

            if (b->pendingDropFlag)
            {
                b->pendingDropFlag = false;
                const Vector3 v = b->pendingDropVel;
                __sync_synchronize();
                if (b->isKinematic)
                {
                    b->setKinematic(false);
                    b->velocity = v;
                    b->wakeUp();
                }
            }
        }
    }

    void PhysicsWorld::snapshotRenderTransforms()
    {
        const int n = bodyCount_;
        for (int i = 0; i < n; ++i)
        {
            RigidBody *b = bodies_[i];
            if (!b)
                continue;
            const int writeIdx = 1 - b->renderIdx;
            b->renderPos[writeIdx] = b->position;
            b->renderRot[writeIdx] = b->orientation;
            __sync_synchronize();
            b->renderIdx = writeIdx;
        }
    }

    void PhysicsWorld::applyKinematicTargets()
    {
        const int n = bodyCount_;
        const float invDt = (fixedTimeStep > 0.0f) ? (1.0f / fixedTimeStep) : 0.0f;

        for (int i = 0; i < n; ++i)
        {
            RigidBody *b = bodies_[i];
            if (!b || !b->isKinematic)
                continue;

            const Vector3 oldPos = b->position;
            const Quaternion oldRot = b->orientation;

            b->position = b->kinematicTargetPos;
            b->orientation = b->kinematicTargetRot;

            const Vector3 delta = b->position - oldPos;
            b->velocity = delta * invDt;

            const float kMaxKinematicVel = 8.0f;
            float velLenSq = b->velocity.lengthSquared();
            if (velLenSq > kMaxKinematicVel * kMaxKinematicVel)
            {
                float invLen = kMaxKinematicVel * FastMath::fastInvSqrt(velLenSq);
                b->velocity = b->velocity * invLen;
            }

            const Quaternion invOld = oldRot.conjugate();
            Quaternion qDelta = b->orientation * invOld;

            if (qDelta.w < 0.0f)
            {
                qDelta.x = -qDelta.x;
                qDelta.y = -qDelta.y;
                qDelta.z = -qDelta.z;
                qDelta.w = -qDelta.w;
            }

            const float wClamped = (qDelta.w > 1.0f) ? 1.0f : (qDelta.w < -1.0f ? -1.0f : qDelta.w);
            const float halfAngle = acosf(wClamped);
            const float sinHalf = sinf(halfAngle);
            if (sinHalf > 1e-6f && invDt > 0.0f)
            {
                const float angle = 2.0f * halfAngle;
                const float invSinHalf = 1.0f / sinHalf;
                b->angularVelocity = Vector3(
                    qDelta.x * invSinHalf * angle * invDt,
                    qDelta.y * invSinHalf * angle * invDt,
                    qDelta.z * invSinHalf * angle * invDt);
            }
            else
            {
                b->angularVelocity = Vector3(0.0f, 0.0f, 0.0f);
            }

            b->updateBoundsFromTransform();

            const AABB &kinBounds = b->bounds;
            for (int j = 0; j < n; ++j)
            {
                if (j == i)
                    continue;
                RigidBody *other = bodies_[j];
                if (!other || other->isStatic || other->isKinematic)
                    continue;
                if (!other->isSleeping)
                    continue;
                if (kinBounds.intersects(other->bounds))
                    other->wakeUp();
            }
        }
    }

    bool PhysicsWorld::addBody(RigidBody *body)
    {
        if (!body)
        {
            LOGE(::pip3D::Debug::LOG_MODULE_PHYSICS,
                 "PhysicsWorld::addBody called with null body");
            return false;
        }
        if (bodyCount_ >= kMaxBodies)
        {
            LOGW(::pip3D::Debug::LOG_MODULE_PHYSICS,
                 "PhysicsWorld::addBody: kMaxBodies (%d) reached, body dropped",
                 kMaxBodies);
            return false;
        }
        bodies_[bodyCount_++] = body;
        return true;
    }

    void PhysicsWorld::removeBody(RigidBody *body)
    {
        if (!body)
            return;

        manifoldPool_.removeBody(body);
        for (int i = constraintCount_ - 1; i >= 0; --i)
        {
            Constraint *constraint = constraints_[i];
            if (constraint && (constraint->a == body || constraint->b == body))
                constraints_[i] = constraints_[--constraintCount_];
        }
        for (int i = 0; i < bodyCount_; ++i)
        {
            if (bodies_[i] == body)
            {
                bodies_[i] = bodies_[--bodyCount_];
                return;
            }
        }
        LOGW(::pip3D::Debug::LOG_MODULE_PHYSICS,
             "PhysicsWorld::removeBody: body %p not registered", (void *)body);
    }

    bool PhysicsWorld::addConstraint(Constraint *c)
    {
        if (!c)
            return false;
        if (constraintCount_ >= kMaxConstraints)
            return false;
        constraints_[constraintCount_++] = c;
        return true;
    }

    void PhysicsWorld::removeConstraint(Constraint *c)
    {
        if (!c)
            return;
        for (int i = 0; i < constraintCount_; ++i)
        {
            if (constraints_[i] == c)
            {
                constraints_[i] = constraints_[--constraintCount_];
                return;
            }
        }
    }

    void PhysicsWorld::addBuoyancyZone(const BuoyancyZone &zone)
    {
        if (waterZoneCount_ >= kMaxWaterZones)
            return;
        waterZones_[waterZoneCount_++] = zone;
    }

    static void sanitizeBody(RigidBody *b) noexcept
    {
        if (!b || !b->isNan())
            return;

        Vector3 fallback(0.0f, 3.0f, 0.0f);
        if (b->position.x == b->position.x && b->position.y == b->position.y && b->position.z == b->position.z)
        {
            fallback = b->position;
            if (fallback.y < 1.0f)
                fallback.y = 3.0f;
        }
        b->sanitize(fallback);
    }

    static int calculateCcdSubsteps(RigidBody *const *bodies, int bodyCount,
                                    float deltaTime) noexcept
    {
        float maxTravel = 0.0f;
        float minDynamicRadius = FLT_MAX;
        for (int i = 0; i < bodyCount; ++i)
        {
            const RigidBody *body = bodies[i];
            if (!body || body->isStatic || body->isKinematic || body->isSleeping ||
                body->invMass <= 0.0f)
                continue;

            const float travel = sqrtf(body->velocity.lengthSquared()) * deltaTime;
            if (travel > maxTravel)
                maxTravel = travel;

            const float sx = fabsf(body->size.x);
            const float sy = fabsf(body->size.y);
            const float sz = fabsf(body->size.z);
            float radius = sx < sy ? sx : sy;
            if (sz < radius)
                radius = sz;
            radius *= 0.5f;
            if (body->shape == BODY_SHAPE_SPHERE || body->shape == BODY_SHAPE_CAPSULE)
                radius = body->radius;
            if (radius > 1e-4f && radius < minDynamicRadius)
                minDynamicRadius = radius;
        }

        if (maxTravel <= 0.0f || minDynamicRadius == FLT_MAX)
            return 1;

        const float maxSafeTravel = minDynamicRadius * 0.5f;
        int substeps = static_cast<int>(ceilf(maxTravel / maxSafeTravel));
        if (substeps < 1)
            substeps = 1;
        if (substeps > PhysicsConfig::MAX_SUBSTEPS)
            substeps = PhysicsConfig::MAX_SUBSTEPS;
        return substeps;
    }

    void PhysicsWorld::preStepJoints(float deltaTime)
    {
        for (int i = 0; i < constraintCount_; ++i)
        {
            Constraint *c = constraints_[i];
            if (c)
                c->preStep(deltaTime);
        }
    }

    void PhysicsWorld::solveJoints(float deltaTime)
    {
        for (int i = 0; i < constraintCount_; ++i)
        {
            Constraint *c = constraints_[i];
            if (c)
                c->solve(deltaTime);
        }
    }

    void PhysicsWorld::stepInternal(float deltaTime)
    {
        const int bodyCount = bodyCount_;
        const float gravityMag = gravityMag_;

        for (int i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies_[i];
            if (!b)
                continue;

            sanitizeBody(b);
            b->beginStep();

            if (!b->isStatic && !b->isKinematic && !b->isSleeping && b->invMass > 0.0f)
            {
                b->acceleration += gravity * b->gravityScale;
            }
        }

        if (waterZoneCount_ > 0)
        {
            const float effectiveGravity = (gravityMag > 0.0f) ? gravityMag : 9.81f;
            applyBuoyancy(bodies_, bodyCount,
                          waterZones_, waterZoneCount_,
                          effectiveGravity, deltaTime);
        }

        for (int i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies_[i];
            if (!b || b->isStatic || b->isSleeping || b->isKinematic)
                continue;

            b->velocity += b->acceleration * deltaTime;
            b->angularVelocity += b->angularAcceleration * deltaTime;

            float linFactor = 1.0f - b->linearDamping * deltaTime;
            float angFactor = 1.0f - b->angularDamping * deltaTime;
            if (linFactor < 0.0f)
                linFactor = 0.0f;
            if (angFactor < 0.0f)
                angFactor = 0.0f;
            b->velocity *= linFactor;
            b->angularVelocity *= angFactor;

            float vLenSq = b->velocity.lengthSquared();
            if (vLenSq > PhysicsConfig::MAX_LINEAR_VELOCITY * PhysicsConfig::MAX_LINEAR_VELOCITY)
            {
                b->velocity *= PhysicsConfig::MAX_LINEAR_VELOCITY * FastMath::fastInvSqrt(vLenSq);
            }
            float angLenSq = b->angularVelocity.lengthSquared();
            if (angLenSq > PhysicsConfig::MAX_ANGULAR_VELOCITY * PhysicsConfig::MAX_ANGULAR_VELOCITY)
            {
                b->angularVelocity *= PhysicsConfig::MAX_ANGULAR_VELOCITY * FastMath::fastInvSqrt(angLenSq);
            }
        }

        for (int mi = 0; mi < ManifoldPool::kMaxManifolds; ++mi)
        {
            if (!manifoldPool_.isActive(mi))
                continue;
            ContactManifold &m = manifoldPool_.getManifold(mi);
            if (m.bodyA && m.bodyB)
            {
                manifoldPool_.refreshManifold(m);
            }
        }

        contactCount_ = 0;

        for (int i = 0; i < bodyCount; ++i)
        {
            RigidBody *a = bodies_[i];
            if (!a)
                continue;
            for (int j = i + 1; j < bodyCount; ++j)
            {
                RigidBody *b = bodies_[j];
                if (!b)
                    continue;

                const bool aActive = !a->isStatic && !a->isSleeping;
                const bool bActive = !b->isStatic && !b->isSleeping;
                if (!aActive && !bActive)
                    continue;
                if (a->isStatic && b->isStatic)
                    continue;
                if (a->isKinematic && b->isKinematic)
                    continue;

                ContactManifold fresh = pip3D::detectCollision(a, b, deltaTime);

                if (fresh.hasCollision && (fresh.bodyA != a || fresh.bodyB != b))
                    fresh.hasCollision = false;

                if (!fresh.hasCollision || fresh.contactCount <= 0)
                    continue;

                if (fresh.contactCount > PhysicsConfig::MAX_CONTACT_POINTS)
                    fresh.contactCount = PhysicsConfig::MAX_CONTACT_POINTS;

                ContactManifold *pm = manifoldPool_.findOrCreate(a, b);
                if (!pm)
                    continue;

                manifoldPool_.mergeContacts(*pm, fresh);

                if (pm->contactCount <= 0)
                    continue;

                pm->bodyAIndex = (uint8_t)i;
                pm->bodyBIndex = (uint8_t)j;
                pm->hasRealContact = false;
                for (int k = 0; k < pm->contactCount; ++k)
                {
                    if (pm->contacts[k].penetration > 0.0f)
                    {
                        pm->hasRealContact = true;
                        break;
                    }
                }

                {
                    float maxPen = 0.0f;
                    for (int k = 0; k < pm->contactCount; ++k)
                        if (pm->contacts[k].penetration > maxPen)
                            maxPen = pm->contacts[k].penetration;

                    const float kDeepThreshold = 0.3f;
                    if (maxPen > kDeepThreshold &&
                        !a->isStatic && !b->isStatic &&
                        !a->isKinematic && !b->isKinematic)
                    {
                        const float invMassA = a->invMass;
                        const float invMassB = b->invMass;
                        const float invMassSum = invMassA + invMassB;
                        if (invMassSum > 0.0f)
                        {
                            float sepDist = (maxPen - kDeepThreshold) * 0.5f;
                            if (sepDist > 0.3f)
                                sepDist = 0.3f;

                            const Vector3 sep = pm->normal * sepDist;
                            const float aFactor = invMassA / invMassSum;
                            const float bFactor = invMassB / invMassSum;

                            a->position -= sep * aFactor;
                            a->velocity = a->velocity * 0.5f;
                            a->angularVelocity = a->angularVelocity * 0.5f;
                            a->updateBoundsFromTransform();

                            b->position += sep * bFactor;
                            b->velocity = b->velocity * 0.5f;
                            b->angularVelocity = b->angularVelocity * 0.5f;
                            b->updateBoundsFromTransform();

                            for (int k = 0; k < pm->contactCount; ++k)
                            {
                                if (pm->contacts[k].penetration > kDeepThreshold * 0.5f)
                                    pm->contacts[k].penetration = kDeepThreshold * 0.5f;
                            }
                        }
                    }
                }

                maybeWake(*pm);

                if (contactCount_ < kMaxContacts)
                {
                    activeContacts_[contactCount_++] = pm;
                }
            }
        }

        const bool hasStatic = true;
        const float hertz = hasStatic ? PhysicsConfig::STATIC_CONTACT_HERTZ
                                      : PhysicsConfig::CONTACT_HERTZ;
        const SoftConstraint soft = makeSoftConstraint(
            hertz, PhysicsConfig::CONTACT_DAMPING_RATIO, deltaTime);

        preStepJoints(deltaTime);

        for (int c = 0; c < contactCount_; ++c)
        {
            solver.preStep(*activeContacts_[c], deltaTime, soft);
        }

        for (int c = 0; c < contactCount_; ++c)
        {
            ContactManifold &info = *activeContacts_[c];
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
            Resolver::buildTangentBasis(n, t1, t2);

            for (int j = 0; j < info.contactCount; ++j)
            {
                Contact &c2 = info.contacts[j];
                const Vector3 impulse = n * c2.accumulatedImpulse +
                                        t1 * c2.tangentImpulse1 +
                                        t2 * c2.tangentImpulse2;
                if (impulse.lengthSquared() <= 1e-12f)
                    continue;

                const Vector3 rA = c2.pos - a->position;
                const Vector3 rB = c2.pos - b->position;

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

        for (int iter = 0; iter < PhysicsConfig::SOLVER_ITERATIONS; ++iter)
        {
            for (int c = 0; c < contactCount_; ++c)
                solver.solve(*activeContacts_[c], deltaTime, true);
            solveJoints(deltaTime);
        }

        constexpr float kPositionBaumgarte = 0.20f;
        constexpr float kMaxPositionCorrection = 0.20f;
        for (int iter = 0; iter < PhysicsConfig::RELAX_ITERATIONS; ++iter)
        {
            for (int c = 0; c < contactCount_; ++c)
            {
                ContactManifold &info = *activeContacts_[c];
                RigidBody *a = info.bodyA;
                RigidBody *b = info.bodyB;
                if (!a || !b || a->isTrigger || b->isTrigger)
                    continue;

                const bool aImmovable = a->isStatic || a->isKinematic || a->isSleeping;
                const bool bImmovable = b->isStatic || b->isKinematic || b->isSleeping;
                const float invMassA = aImmovable ? 0.0f : a->invMass;
                const float invMassB = bImmovable ? 0.0f : b->invMass;
                if (invMassA + invMassB <= 0.0f)
                    continue;

                const Vector3 n = info.normal;
                for (int ci = 0; ci < info.contactCount; ++ci)
                {
                    Contact &ct = info.contacts[ci];
                    const Vector3 pointA = a->position + a->orientation.rotate(ct.localPointA);
                    const Vector3 pointB = b->position + b->orientation.rotate(ct.localPointB);
                    const Vector3 rA = pointA - a->position;
                    const Vector3 rB = pointB - b->position;

                    float separation = (pointB - pointA).dot(n);
                    float error = separation + PhysicsConfig::POSITION_SLOP;
                    if (error >= 0.0f)
                        continue;
                    if (error < -kMaxPositionCorrection)
                        error = -kMaxPositionCorrection;

                    const float normalMass = Resolver::effectiveMass(
                        a, b, rA, rB, n, invMassA, invMassB);
                    if (normalMass <= 0.0f)
                        continue;

                    const Vector3 pseudoVA = a->posCorrectionVelocity +
                                             a->posCorrectionAngular.cross(rA);
                    const Vector3 pseudoVB = b->posCorrectionVelocity +
                                             b->posCorrectionAngular.cross(rB);
                    const float pseudoVn = (pseudoVB - pseudoVA).dot(n);
                    float impulseMag = -normalMass *
                                       (pseudoVn + (kPositionBaumgarte / deltaTime) * error);
                    if (impulseMag <= 0.0f)
                        continue;

                    const Vector3 impulse = n * impulseMag;
                    if (!aImmovable)
                    {
                        a->posCorrectionVelocity -= impulse * invMassA;
                        a->posCorrectionAngular -=
                            a->mulWorldInvInertia(rA.cross(impulse));
                    }
                    if (!bImmovable)
                    {
                        b->posCorrectionVelocity += impulse * invMassB;
                        b->posCorrectionAngular +=
                            b->mulWorldInvInertia(rB.cross(impulse));
                    }
                }
            }
        }

        for (int i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies_[i];
            if (b)
                b->integratePseudoVelocity(deltaTime);
        }

        for (int iter = 0; iter < PhysicsConfig::RELAX_ITERATIONS; ++iter)
        {
            for (int c = 0; c < contactCount_; ++c)
                solver.solve(*activeContacts_[c], deltaTime, false);
        }

        applyRestingVelocityZeroing();

        for (int i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies_[i];
            if (!b || b->isStatic || b->isSleeping || b->isKinematic)
                continue;

            b->position += b->velocity * deltaTime;

            float angLenSq = b->angularVelocity.lengthSquared();
            if (angLenSq > 1e-8f)
            {
                const float halfDt = 0.5f * deltaTime;
                const Quaternion omega(
                    b->angularVelocity.x * halfDt,
                    b->angularVelocity.y * halfDt,
                    b->angularVelocity.z * halfDt,
                    0.0f);
                const Quaternion deltaQ = omega * b->orientation;
                b->orientation.x += deltaQ.x;
                b->orientation.y += deltaQ.y;
                b->orientation.z += deltaQ.z;
                b->orientation.w += deltaQ.w;
                b->orientation.normalize();
                b->updateWorldInvInertia();
            }

            b->updateBoundsFromTransform();
        }

        updateSleepAndSettle(deltaTime);

        manifoldPool_.pruneEmpty();

        for (int i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies_[i];
            if (!b)
                continue;
            sanitizeBody(b);
        }
    }

    float PhysicsWorld::getInterpolationAlpha() const
    {
        if (fixedTimeStep <= 0.0f)
            return 0.0f;
        float a = pendingFrameDelta_ / fixedTimeStep;
        if (a < 0.0f)
            a = 0.0f;
        if (a > 1.0f)
            a = 1.0f;
        return a;
    }

    void PhysicsWorld::interpolateTransforms(float alpha)
    {
        (void)alpha;
    }

    void PhysicsWorld::updateFixed(float frameDelta)
    {
        const float maxAccum = fixedTimeStep * static_cast<float>(PhysicsConfig::MAX_SUBSTEPS);
        float newPending = pendingFrameDelta_ + frameDelta;
        if (newPending > maxAccum)
            newPending = maxAccum;
        pendingFrameDelta_ = newPending;
        __sync_synchronize();
#if PIP3D_TARGET_ESP32
        if (physicsTask_)
            xTaskNotifyGive(physicsTask_);
#endif
    }

    void PhysicsWorld::runStepJob(float deltaTime)
    {
        if (deltaTime <= 0.0f)
            return;

        const int substeps = calculateCcdSubsteps(bodies_, bodyCount_, deltaTime);
        const float substepDelta = deltaTime / static_cast<float>(substeps);
        for (int step = 0; step < substeps; ++step)
            stepInternal(substepDelta);
    }

    void PhysicsWorld::maybeWake(ContactManifold &info)
    {
        RigidBody *a = info.bodyA;
        RigidBody *b = info.bodyB;
        if (!a || !b)
            return;
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
        if (contactCount_ == 0)
            return;

        const int bodyCount = bodyCount_;
        const int contactCount = contactCount_;

        for (int i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies_[i];
            if (!b || b->isStatic || b->isKinematic || b->isSleeping)
                continue;

            Vector3 normals[16];
            int normalCount = 0;

            for (int c = 0; c < contactCount && normalCount < 16; ++c)
            {
                const ContactManifold *info = activeContacts_[c];
                if (!info || !info->hasRealContact)
                    continue;

                if (info->bodyAIndex == i)
                {
                    normals[normalCount++] = info->normal * -1.0f;
                }
                else if (info->bodyBIndex == i)
                {
                    normals[normalCount++] = info->normal;
                }
            }
            if (normalCount == 0)
                continue;

            float v2 = b->velocity.lengthSquared();
            float w2 = b->angularVelocity.lengthSquared();

            if (v2 < PhysicsConfig::RESTING_LINEAR_ZERO_SQ * 4.0f)
            {
                for (int n = 0; n < normalCount; ++n)
                {
                    float vn = b->velocity.dot(normals[n]);
                    if (vn < 0.0f && vn * vn < PhysicsConfig::RESTING_LINEAR_ZERO_SQ)
                        b->velocity -= normals[n] * vn;
                }
            }

            bool gravityDrivesAlongContact = false;
            const float gravitySq = gravity.lengthSquared();
            for (int n = 0; n < normalCount; ++n)
            {
                const Vector3 tangentGravity = gravity - normals[n] * gravity.dot(normals[n]);
                if (tangentGravity.lengthSquared() > gravitySq * 0.0025f)
                {
                    gravityDrivesAlongContact = true;
                    break;
                }
            }

            if (!gravityDrivesAlongContact && w2 < PhysicsConfig::RESTING_ANGULAR_ZERO_SQ)
                b->angularVelocity = Vector3(0, 0, 0);
        }
    }

    void PhysicsWorld::updateSleepAndSettle(float deltaTime)
    {
        const int bodyCount = bodyCount_;
        const int contactCount = contactCount_;
        const float kSleepPenLimit = 0.08f;

        for (int i = 0; i < bodyCount; ++i)
        {
            sleepSupported_[i] = false;
            sleepDeepPen_[i] = false;
            sleepGravityDriven_[i] = false;
        }

        const float gravitySq = gravity.lengthSquared();
        auto markGravityDriven = [&](int bodyIndex, const Vector3 &supportNormal) noexcept
        {
            if (bodyIndex < 0 || bodyIndex >= bodyCount || gravitySq <= 1e-12f)
                return;
            const Vector3 tangentGravity = gravity - supportNormal * gravity.dot(supportNormal);
            if (tangentGravity.lengthSquared() > gravitySq * 0.0025f)
                sleepGravityDriven_[bodyIndex] = true;
        };

        for (int c = 0; c < contactCount; ++c)
        {
            const ContactManifold *info = activeContacts_[c];
            if (!info || !info->hasRealContact)
                continue;

            float maxPen = 0.0f;
            for (int k = 0; k < info->contactCount; ++k)
                if (info->contacts[k].penetration > maxPen)
                    maxPen = info->contacts[k].penetration;

            RigidBody *a = info->bodyA;
            RigidBody *b = info->bodyB;
            const int ia = info->bodyAIndex;
            const int ib = info->bodyBIndex;

            if (b && (b->isStatic || b->isSleeping) && ia < bodyCount)
            {
                sleepSupported_[ia] = true;
                markGravityDriven(ia, -info->normal);
            }
            if (a && (a->isStatic || a->isSleeping) && ib < bodyCount)
            {
                sleepSupported_[ib] = true;
                markGravityDriven(ib, info->normal);
            }

            if (maxPen > kSleepPenLimit)
            {
                if (ia < bodyCount)
                    sleepDeepPen_[ia] = true;
                if (ib < bodyCount)
                    sleepDeepPen_[ib] = true;
            }
        }

        for (int i = 0; i < bodyCount; ++i)
        {
            RigidBody *b = bodies_[i];
            if (b->isStatic || b->isKinematic || !b->canSleep)
                continue;

            float v2 = b->velocity.lengthSquared();
            float w2 = b->angularVelocity.lengthSquared();

            if (sleepSupported_[i] && !sleepGravityDriven_[i])
            {
                if (v2 < 0.0025f)
                {
                    b->velocity = Vector3(0.0f, 0.0f, 0.0f);
                    v2 = 0.0f;
                }
                if (w2 < 0.0025f)
                {
                    b->angularVelocity = Vector3(0.0f, 0.0f, 0.0f);
                    w2 = 0.0f;
                }
            }

            if (v2 > PhysicsConfig::SLEEP_LINEAR_THRESHOLD_SQ ||
                w2 > PhysicsConfig::SLEEP_ANGULAR_THRESHOLD_SQ)
            {
                b->sleepTimer = 0.0f;
                b->isSleeping = false;
                continue;
            }

            if (v2 < PhysicsConfig::SLEEP_LINEAR_THRESHOLD_SQ &&
                w2 < PhysicsConfig::SLEEP_ANGULAR_THRESHOLD_SQ)
            {
                if (sleepGravityDriven_[i])
                {
                    b->sleepTimer = 0.0f;
                    b->isSleeping = false;
                    continue;
                }
                if (!sleepSupported_[i])
                {
                    b->sleepTimer = 0.0f;
                    if (b->isSleeping)
                        b->wakeUp();
                    continue;
                }
                if (sleepDeepPen_[i])
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

    void PhysicsWorld::debugDraw()
    {
        for (int i = 0; i < bodyCount_; ++i)
        {
            RigidBody *b = bodies_[i];
            if (!b)
                continue;

            const Vector3 pos = b->isKinematic ? b->kinematicTargetPos : b->getRenderPos();
            const Quaternion rot = b->isKinematic ? b->kinematicTargetRot : b->getRenderRot();

            uint16_t shapeColor = Color::GREEN;
            if (b->isStatic)
                shapeColor = Color::RED;
            else if (b->isKinematic)
                shapeColor = Color::YELLOW;
            else if (b->isSleeping)
                shapeColor = Color::GRAY;

            if (b->isStatic)
            {
                const uint16_t aabbColor = Color::rgb(80, 40, 40);
                DBG_AABB(b->bounds, aabbColor, ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
            }

            if (b->shape == BODY_SHAPE_SPHERE)
            {
                DBG_SPHERE(pos, b->radius, shapeColor,
                           ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
            }
            else if (b->shape == BODY_SHAPE_CAPSULE)
            {
                const float endOffset = b->capsuleHalfHeight + b->radius;
                Vector3 axisY = rot.rotate(Vector3(0.0f, endOffset, 0.0f));
                Vector3 p0 = pos - axisY;
                Vector3 p1 = pos + axisY;
                DBG_CAPSULE(p0, p1, b->radius, shapeColor,
                            ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
            }
            else if (b->shape == BODY_SHAPE_CYLINDER || b->shape == BODY_SHAPE_CONVEX)
            {
                Vector3 wv[RigidBody::kMaxConvexVerts];
                const int vc = b->convexCount;
                for (int vi = 0; vi < vc; ++vi)
                    wv[vi] = pos + rot.rotate(b->convexVerts[vi]);

                static const Vector3 kDirs[14] = {
                    Vector3(1, 0, 0), Vector3(-1, 0, 0),
                    Vector3(0, 1, 0), Vector3(0, -1, 0),
                    Vector3(0, 0, 1), Vector3(0, 0, -1),
                    Vector3(0.57735f, 0.57735f, 0.57735f),
                    Vector3(-0.57735f, 0.57735f, 0.57735f),
                    Vector3(0.57735f, -0.57735f, 0.57735f),
                    Vector3(-0.57735f, -0.57735f, 0.57735f),
                    Vector3(0.57735f, 0.57735f, -0.57735f),
                    Vector3(-0.57735f, 0.57735f, -0.57735f),
                    Vector3(0.57735f, -0.57735f, -0.57735f),
                    Vector3(-0.57735f, -0.57735f, -0.57735f)};

                for (int d = 0; d < 14; ++d)
                {
                    float bestDot0 = -FLT_MAX;
                    float bestDot1 = -FLT_MAX;
                    int bestIdx0 = 0;
                    int bestIdx1 = 0;
                    for (int vi = 0; vi < vc; ++vi)
                    {
                        const float dot = wv[vi].dot(kDirs[d]);
                        if (dot > bestDot0)
                        {
                            bestDot1 = bestDot0;
                            bestIdx1 = bestIdx0;
                            bestDot0 = dot;
                            bestIdx0 = vi;
                        }
                        else if (dot > bestDot1)
                        {
                            bestDot1 = dot;
                            bestIdx1 = vi;
                        }
                    }
                    if (bestIdx0 != bestIdx1)
                    {
                        DBG_LINE(wv[bestIdx0], wv[bestIdx1], shapeColor,
                                 ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
                    }
                }
            }
            else
            {
                DBG_OBB(pos, b->size * 0.5f, rot, shapeColor,
                        ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
            }
        }

        int contactMarkersDrawn = 0;
        const int kMaxContactMarkers = 32;
        for (int mi = 0; mi < ManifoldPool::kMaxManifolds && contactMarkersDrawn < kMaxContactMarkers; ++mi)
        {
            if (!manifoldPool_.isActive(mi))
                continue;
            const ContactManifold &info = manifoldPool_.getManifold(mi);
            if (!info.hasCollision || info.contactCount <= 0)
                continue;

            Vector3 n = info.normal;
            float nLenSq = n.lengthSquared();
            if (nLenSq > 1e-8f)
                n *= FastMath::fastInvSqrt(nLenSq);

            for (int j = 0; j < info.contactCount && contactMarkersDrawn < kMaxContactMarkers; ++j)
            {
                const Contact &c = info.contacts[j];
                if (c.penetration <= 0.0f)
                    continue;
                DBG_SPHERE(c.pos, 0.04f, Color::RED,
                           ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
                DBG_ARROW(c.pos, n, 0.12f, 0.025f, Color::YELLOW,
                          ::pip3D::Debug::DEBUG_CATEGORY_PHYSICS);
                ++contactMarkersDrawn;
            }
        }
    }

}
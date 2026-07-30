#pragma once

#include "Core/Platform.hpp"
#include "Core/Jobs.hpp"
#include "Math/Collision.hpp"

#include "Physics/Types.hpp"
#include "Physics/RigidBody/Body.hpp"
#include "Physics/RigidBody/Contacts.hpp"
#include "Physics/RigidBody/Constraints.hpp"
#include "Physics/RigidBody/Buoyancy.hpp"
#include "Physics/RigidBody/Resolver.hpp"
#include "Physics/Manifold.hpp"
#include "Physics/Collision/Narrowphase.hpp"
#include "Physics/Collision/Raycast.hpp"

#if PIP3D_TARGET_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace pip3D
{
        class PhysicsWorld
        {
        public:
                static constexpr int kMaxBodies = 48;
                static constexpr int kMaxConstraints = 24;
                static constexpr int kMaxWaterZones = 8;
                static constexpr int kMaxContacts = 32;

        private:
                RigidBody *bodies_[kMaxBodies];
                int bodyCount_;

                Constraint *constraints_[kMaxConstraints];
                int constraintCount_;

                BuoyancyZone waterZones_[kMaxWaterZones];
                int waterZoneCount_;

                ManifoldPool manifoldPool_;

                ContactManifold *activeContacts_[kMaxContacts];
                int contactCount_;

                Resolver solver;

                Vector3 gravity;
                float gravityMag_;

                float fixedTimeStep;
                float accumulator;

                bool sleepSupported_[kMaxBodies];
                bool sleepDeepPen_[kMaxBodies];
                bool sleepGravityDriven_[kMaxBodies];

                volatile float pendingFrameDelta_ = 0.0f;

#if PIP3D_TARGET_ESP32
                TaskHandle_t physicsTask_ = nullptr;
#endif
                volatile bool physicsStop_ = false;

                void stepInternal(float deltaTime);
                void runStepJob(float deltaTime);

                void maybeWake(ContactManifold &info);
                void updateSleepAndSettle(float deltaTime);
                void applyRestingVelocityZeroing();
                void applyKinematicTargets();

                void preStepJoints(float deltaTime);
                void solveJoints(float deltaTime);

#if PIP3D_TARGET_ESP32
                static void physicsTaskEntry(void *userData);
                void spawnPhysicsTask();
#endif
                void snapshotRenderTransforms();
                void processPendingCommands();

        public:
                PhysicsWorld();
                ~PhysicsWorld();

                PhysicsWorld(const PhysicsWorld &) = delete;
                PhysicsWorld &operator=(const PhysicsWorld &) = delete;
                PhysicsWorld(PhysicsWorld &&) = delete;
                PhysicsWorld &operator=(PhysicsWorld &&) = delete;

                bool addBody(RigidBody *body);
                void removeBody(RigidBody *body);
                bool addConstraint(Constraint *c);
                void removeConstraint(Constraint *c);
                void addBuoyancyZone(const BuoyancyZone &zone);

                PIP3D_FORCE_INLINE int getBodyCount() const { return bodyCount_; }
                PIP3D_FORCE_INLINE int getConstraintCount() const { return constraintCount_; }
                PIP3D_FORCE_INLINE int getWaterZoneCount() const { return waterZoneCount_; }
                PIP3D_FORCE_INLINE int getContactCount() const { return contactCount_; }

                PIP3D_FORCE_INLINE RigidBody *getBody(int i) { return (i >= 0 && i < bodyCount_) ? bodies_[i] : nullptr; }
                PIP3D_FORCE_INLINE const RigidBody *getBody(int i) const { return (i >= 0 && i < bodyCount_) ? bodies_[i] : nullptr; }

                PIP3D_FORCE_INLINE ManifoldPool &getManifoldPool() { return manifoldPool_; }
                PIP3D_FORCE_INLINE const ManifoldPool &getManifoldPool() const { return manifoldPool_; }

                PIP3D_FORCE_INLINE ContactManifold *getContact(int i) { return (i >= 0 && i < contactCount_) ? activeContacts_[i] : nullptr; }
                PIP3D_FORCE_INLINE const ContactManifold *getContact(int i) const { return (i >= 0 && i < contactCount_) ? activeContacts_[i] : nullptr; }

                void setGravity(const Vector3 &g)
                {
                        gravity = g;
                        gravityMag_ = g.length();
                }
                PIP3D_FORCE_INLINE const Vector3 &getGravity() const { return gravity; }
                PIP3D_FORCE_INLINE float getGravityMag() const { return gravityMag_; }

                PIP3D_FORCE_INLINE bool isAsyncEnabled() const { return true; }

                void setFixedTimeStep(float dt) { fixedTimeStep = dt; }
                PIP3D_FORCE_INLINE float getFixedTimeStep() const { return fixedTimeStep; }

                void updateFixed(float frameDelta);

                void interpolateTransforms(float alpha);
                float getInterpolationAlpha() const;

                PIP3D_FORCE_INLINE bool ensureIdle() { return true; }
                PIP3D_FORCE_INLINE void releaseIdle() {}

                PIP3D_FORCE_INLINE void finishStep() {}

                bool raycast(const Ray &ray, RaycastHit &outHit,
                             float maxDistance = FLT_MAX);

                void debugDraw();
        };
}
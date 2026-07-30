#pragma once

#include "Core/Platform.hpp"
#include "Math/Collision.hpp"
#include "Physics/Types.hpp"

namespace pip3D
{

    struct alignas(4) ConvexFace
    {
        Vector3 normal;
        float offset;
        uint8_t vertIdx[12];
        uint8_t vertCount;
        uint8_t _pad[3];
    };
    static_assert(sizeof(ConvexFace) == 32, "ConvexFace layout drifted");
    static_assert(alignof(ConvexFace) == 4, "ConvexFace align drifted");

    [[nodiscard]] bool computeConvexHullFaces(const Vector3 *verts, int vertCount,
                                              ConvexFace *outFaces, int *outFaceCount,
                                              int maxFaces) noexcept;

    struct alignas(16) RigidBody
    {

        Vector3 position;
        Vector3 previousPosition;
        Vector3 velocity;
        Vector3 acceleration;
        Vector3 angularVelocity;
        Vector3 angularAcceleration;
        Quaternion orientation;
        Quaternion previousOrientation;

        Vector3 posCorrectionVelocity;
        Vector3 posCorrectionAngular;

        Vector3 renderPos[2];
        Quaternion renderRot[2];
        volatile int renderIdx;

        Vector3 kinematicTargetPos;
        Quaternion kinematicTargetRot;

        volatile bool pendingGrabFlag;
        volatile bool pendingDropFlag;
        Vector3 pendingDropVel;

        Vector3 size;
        float radius;
        float capsuleHalfHeight;
        BodyShape shape;

        static constexpr int kMaxConvexVerts = 32;

        static constexpr int kMaxConvexFaces = 24;
        Vector3 convexVerts[kMaxConvexVerts];
        ConvexFace convexFaces[kMaxConvexFaces];
        int convexCount;
        int convexFaceCount;
        bool convexHullComplete;

    private:
        float mass_;

        float savedMass_ = 0.0f;

    public:
        float invMass;
        Vector3 invInertia;
        float worldInvInertia[9];

        float restitution;
        float friction;

        float linearDamping;
        float angularDamping;
        float gravityScale;

        bool isStatic;
        bool isKinematic;
        bool isTrigger;
        bool canSleep;
        bool isSleeping;
        float sleepTimer;

        AABB bounds;

        RigidBody()
            : RigidBody(Vector3(0, 0, 0), Vector3(1, 1, 1), 1.0f) {}

        RigidBody(const Vector3 &pos, const Vector3 &size_, float m = 1.0f)
            : position(pos), previousPosition(pos),
              velocity(0, 0, 0), acceleration(0, 0, 0),
              angularVelocity(0, 0, 0), angularAcceleration(0, 0, 0),
              orientation(), previousOrientation(),
              posCorrectionVelocity(0, 0, 0), posCorrectionAngular(0, 0, 0),
              renderPos{pos, pos}, renderRot{Quaternion(), Quaternion()},
              renderIdx(0),
              kinematicTargetPos(pos), kinematicTargetRot(),
              pendingGrabFlag(false), pendingDropFlag(false),
              pendingDropVel(0, 0, 0),
              size(size_), radius(size_.x * 0.5f), capsuleHalfHeight(0.0f),
              shape(BODY_SHAPE_BOX),
              convexCount(0), convexFaceCount(0), convexHullComplete(false),
              mass_(m > 0.0f ? m : 0.0f),
              savedMass_(0.0f),
              invMass(m > 0.0f ? 1.0f / m : 0.0f),
              invInertia(1.0f, 1.0f, 1.0f),
              restitution(0.5f), friction(0.5f),
              linearDamping(PhysicsConfig::LINEAR_DAMPING),
              angularDamping(PhysicsConfig::ANGULAR_DAMPING),
              gravityScale(1.0f),
              isStatic(false), isKinematic(false), isTrigger(false),
              canSleep(true), isSleeping(false), sleepTimer(0.0f),
              bounds(AABB::fromCenterSize(pos, size_))
        {
            for (int i = 0; i < kMaxConvexVerts; ++i)
                convexVerts[i] = Vector3(0, 0, 0);
            for (int i = 0; i < kMaxConvexFaces; ++i)
            {
                convexFaces[i].normal = Vector3(0, 1, 0);
                convexFaces[i].offset = 0.0f;
                convexFaces[i].vertCount = 0;
            }
            computeInertia();
            updateWorldInvInertia();
        }

        PIP3D_FORCE_INLINE const Vector3 &getRenderPos() const noexcept
        {
            return renderPos[renderIdx];
        }
        PIP3D_FORCE_INLINE const Quaternion &getRenderRot() const noexcept
        {
            return renderRot[renderIdx];
        }

        PIP3D_FORCE_INLINE Vector3
        mulWorldInvInertia(const Vector3 &v) const noexcept
        {
            return Vector3(
                worldInvInertia[0] * v.x + worldInvInertia[1] * v.y + worldInvInertia[2] * v.z,
                worldInvInertia[3] * v.x + worldInvInertia[4] * v.y + worldInvInertia[5] * v.z,
                worldInvInertia[6] * v.x + worldInvInertia[7] * v.y + worldInvInertia[8] * v.z);
        }

        PIP3D_FORCE_INLINE void updateWorldInvInertia() noexcept
        {
            if (invMass <= 0.0f)
            {
                for (int i = 0; i < 9; ++i)
                    worldInvInertia[i] = 0.0f;
                return;
            }

            const float d0 = invInertia.x;
            const float d1 = invInertia.y;
            const float d2 = invInertia.z;

            const Vector3 ex = orientation.rotate(Vector3(1.0f, 0.0f, 0.0f));
            const Vector3 ey = orientation.rotate(Vector3(0.0f, 1.0f, 0.0f));
            const Vector3 ez = orientation.rotate(Vector3(0.0f, 0.0f, 1.0f));

            worldInvInertia[0] = d0 * ex.x * ex.x + d1 * ey.x * ey.x + d2 * ez.x * ez.x;
            worldInvInertia[1] = d0 * ex.x * ex.y + d1 * ey.x * ey.y + d2 * ez.x * ez.y;
            worldInvInertia[2] = d0 * ex.x * ex.z + d1 * ey.x * ey.z + d2 * ez.x * ez.z;

            worldInvInertia[3] = worldInvInertia[1];
            worldInvInertia[4] = d0 * ex.y * ex.y + d1 * ey.y * ey.y + d2 * ez.y * ez.y;
            worldInvInertia[5] = d0 * ex.y * ex.z + d1 * ey.y * ey.z + d2 * ez.y * ez.z;

            worldInvInertia[6] = worldInvInertia[2];
            worldInvInertia[7] = worldInvInertia[5];
            worldInvInertia[8] = d0 * ex.z * ex.z + d1 * ey.z * ey.z + d2 * ez.z * ez.z;
        }

        PIP3D_FORCE_INLINE void applyForce(const Vector3 &force) noexcept
        {
            if (!isStatic && !isKinematic && invMass > 0.0f)
            {
                acceleration += force * invMass;
                wakeUpInternal();
            }
        }

        PIP3D_FORCE_INLINE void
        applyForceAt(const Vector3 &force, const Vector3 &worldPoint) noexcept
        {
            if (!isStatic && !isKinematic && invMass > 0.0f)
            {
                acceleration += force * invMass;
                const Vector3 r = worldPoint - position;
                angularAcceleration += mulWorldInvInertia(r.cross(force));
                wakeUpInternal();
            }
        }

        PIP3D_FORCE_INLINE void applyTorque(const Vector3 &torque) noexcept
        {
            if (!isStatic && !isKinematic && invMass > 0.0f)
            {
                angularAcceleration += mulWorldInvInertia(torque);
                wakeUpInternal();
            }
        }

        PIP3D_FORCE_INLINE void applyImpulse(const Vector3 &impulse) noexcept
        {
            if (!isStatic && !isKinematic && invMass > 0.0f)
            {
                velocity += impulse * invMass;
                wakeUpInternal();
            }
        }

        PIP3D_FORCE_INLINE void
        applyImpulseAt(const Vector3 &impulse, const Vector3 &worldPoint) noexcept
        {
            if (!isStatic && !isKinematic && invMass > 0.0f)
            {
                const Vector3 r = worldPoint - position;
                velocity += impulse * invMass;
                angularVelocity += mulWorldInvInertia(r.cross(impulse));
                wakeUpInternal();
            }
        }

        PIP3D_FORCE_INLINE void applyAngularImpulse(const Vector3 &angularImpulse) noexcept
        {
            if (!isStatic && !isKinematic && invMass > 0.0f)
            {
                angularVelocity += mulWorldInvInertia(angularImpulse);
                wakeUpInternal();
            }
        }

        PIP3D_FORCE_INLINE Vector3
        getPointVelocity(const Vector3 &worldPoint) const noexcept
        {
            return velocity + angularVelocity.cross(worldPoint - position);
        }

        void setBox(const Vector3 &newSize) noexcept
        {
            if (newSize.x <= 0.0f || newSize.y <= 0.0f || newSize.z <= 0.0f)
                return;
            size = newSize;
            shape = BODY_SHAPE_BOX;
            radius = newSize.x * 0.5f;
            capsuleHalfHeight = 0.0f;
            convexCount = 0;
            convexFaceCount = 0;
            convexHullComplete = false;
            updateBoundsFromTransform();
            computeInertia();
            updateWorldInvInertia();
        }

        void setSphere(float r) noexcept
        {
            if (r <= 0.0f)
                return;
            shape = BODY_SHAPE_SPHERE;
            radius = r;
            size = Vector3(r * 2.0f, r * 2.0f, r * 2.0f);
            capsuleHalfHeight = 0.0f;
            convexCount = 0;
            convexFaceCount = 0;
            convexHullComplete = false;
            updateBoundsFromTransform();
            computeInertia();
            updateWorldInvInertia();
        }

        void setCapsule(float r, float halfHeight) noexcept
        {
            if (r <= 0.0f || halfHeight < 0.0f)
                return;
            shape = BODY_SHAPE_CAPSULE;
            radius = r;
            capsuleHalfHeight = halfHeight;
            size = Vector3(r * 2.0f, 2.0f * (halfHeight + r), r * 2.0f);
            convexCount = 0;
            convexFaceCount = 0;
            convexHullComplete = false;
            updateBoundsFromTransform();
            computeInertia();
            updateWorldInvInertia();
        }

        void setCylinder(float r, float halfHeight) noexcept
        {
            if (r <= 0.0f || halfHeight < 0.0f)
                return;

            const float kMinHalfHeight = 0.05f;
            const float effectiveHalfHeight = (halfHeight < kMinHalfHeight) ? kMinHalfHeight : halfHeight;
            const bool isThinDisc = (halfHeight < kMinHalfHeight);

            shape = BODY_SHAPE_CYLINDER;
            radius = r;

            capsuleHalfHeight = effectiveHalfHeight;
            size = Vector3(r * 2.0f, 2.0f * effectiveHalfHeight, r * 2.0f);

            const int kSegments = isThinDisc ? 8 : 12;
            Vector3 verts[kMaxConvexVerts];
            int vCount = 0;
            for (int i = 0; i < kSegments; ++i)
            {
                const float ang = (float(i) / float(kSegments)) * 6.2831853f;
                const float cx = r * cosf(ang);
                const float cz = r * sinf(ang);
                verts[vCount++] = Vector3(cx, effectiveHalfHeight, cz);
                verts[vCount++] = Vector3(cx, -effectiveHalfHeight, cz);
            }

            convexCount = (vCount > kMaxConvexVerts) ? kMaxConvexVerts : vCount;
            for (int i = 0; i < convexCount; ++i)
                convexVerts[i] = verts[i];
            for (int i = convexCount; i < kMaxConvexVerts; ++i)
                convexVerts[i] = Vector3(0, 0, 0);

            convexHullComplete = computeConvexHullFaces(
                convexVerts, convexCount, convexFaces, &convexFaceCount, kMaxConvexFaces);

            updateBoundsFromTransform();
            computeInertia();
            updateWorldInvInertia();
        }

        [[nodiscard]] bool setConvex(const Vector3 *verts, int count) noexcept;

        Vector3 support(const Vector3 &dirWorld) const noexcept
        {
            const Vector3 localDir = orientation.conjugate().rotate(dirWorld);

            if (shape == BODY_SHAPE_SPHERE)
            {
                const float lenSq = localDir.lengthSquared();
                const Vector3 localP = (lenSq > 1e-12f)
                                           ? localDir * (radius * FastMath::fastInvSqrt(lenSq))
                                           : Vector3(0, radius, 0);
                return position + orientation.rotate(localP);
            }

            if (shape == BODY_SHAPE_CAPSULE)
            {
                Vector3 localP;
                localP.y = (localDir.y >= 0.0f) ? capsuleHalfHeight : -capsuleHalfHeight;
                const float lenSq = localDir.lengthSquared();
                if (lenSq > 1e-12f)
                {
                    const float invLen = FastMath::fastInvSqrt(lenSq);
                    localP.x += radius * localDir.x * invLen;
                    localP.y += radius * localDir.y * invLen;
                    localP.z += radius * localDir.z * invLen;
                }
                return position + orientation.rotate(localP);
            }

            if (shape == BODY_SHAPE_CYLINDER || shape == BODY_SHAPE_CONVEX)
            {
                if (convexCount == 0)
                    return position;
                float bestDot = localDir.dot(convexVerts[0]);
                int bestIdx = 0;
                for (int i = 1; i < convexCount; ++i)
                {
                    const float d = localDir.dot(convexVerts[i]);
                    if (d > bestDot)
                    {
                        bestDot = d;
                        bestIdx = i;
                    }
                }
                return position + orientation.rotate(convexVerts[bestIdx]);
            }

            const Vector3 half = size * 0.5f;
            const Vector3 localP(
                (localDir.x >= 0.0f) ? half.x : -half.x,
                (localDir.y >= 0.0f) ? half.y : -half.y,
                (localDir.z >= 0.0f) ? half.z : -half.z);
            return position + orientation.rotate(localP);
        }

        void updateBoundsFromTransform() noexcept
        {
            if (shape == BODY_SHAPE_SPHERE)
            {
                bounds = AABB::fromCenterSize(position, size);
                return;
            }

            Vector3 half;
            if (shape == BODY_SHAPE_CAPSULE)
            {
                half.x = radius;
                half.y = capsuleHalfHeight + radius;
                half.z = radius;
            }
            else if (shape == BODY_SHAPE_CYLINDER || shape == BODY_SHAPE_CONVEX)
            {
                if (convexCount > 0)
                {
                    const Vector3 *v = convexVerts;
                    Vector3 mn = v[0];
                    Vector3 mx = v[0];
                    for (int i = 1; i < convexCount; ++i)
                    {
                        if (v[i].x < mn.x)
                            mn.x = v[i].x;
                        else if (v[i].x > mx.x)
                            mx.x = v[i].x;
                        if (v[i].y < mn.y)
                            mn.y = v[i].y;
                        else if (v[i].y > mx.y)
                            mx.y = v[i].y;
                        if (v[i].z < mn.z)
                            mn.z = v[i].z;
                        else if (v[i].z > mx.z)
                            mx.z = v[i].z;
                    }
                    Vector3 localHalf = (mx - mn) * 0.5f;
                    if (shape == BODY_SHAPE_CYLINDER && localHalf.y < capsuleHalfHeight + 0.05f)
                        localHalf.y = capsuleHalfHeight + 0.05f;
                    const Vector3 ex = orientation.rotate(Vector3(1.0f, 0.0f, 0.0f));
                    const Vector3 ey = orientation.rotate(Vector3(0.0f, 1.0f, 0.0f));
                    const Vector3 ez = orientation.rotate(Vector3(0.0f, 0.0f, 1.0f));
                    const float r0 = fabsf(ex.x) * localHalf.x + fabsf(ey.x) * localHalf.y + fabsf(ez.x) * localHalf.z;
                    const float r1 = fabsf(ex.y) * localHalf.x + fabsf(ey.y) * localHalf.y + fabsf(ez.y) * localHalf.z;
                    const float r2 = fabsf(ex.z) * localHalf.x + fabsf(ey.z) * localHalf.y + fabsf(ez.z) * localHalf.z;
                    const Vector3 r(r0, r1, r2);
                    bounds.min = position - r;
                    bounds.max = position + r;
                    return;
                }
                half.x = radius;
                half.y = capsuleHalfHeight;
                half.z = radius;
            }
            else
            {
                half = size * 0.5f;
            }

            const Vector3 ex = orientation.rotate(Vector3(1.0f, 0.0f, 0.0f));
            const Vector3 ey = orientation.rotate(Vector3(0.0f, 1.0f, 0.0f));
            const Vector3 ez = orientation.rotate(Vector3(0.0f, 0.0f, 1.0f));

            const float r0 = fabsf(ex.x) * half.x + fabsf(ey.x) * half.y + fabsf(ez.x) * half.z;
            const float r1 = fabsf(ex.y) * half.x + fabsf(ey.y) * half.y + fabsf(ez.y) * half.z;
            const float r2 = fabsf(ex.z) * half.x + fabsf(ey.z) * half.y + fabsf(ez.z) * half.z;

            const Vector3 r(r0, r1, r2);
            bounds.min = position - r;
            bounds.max = position + r;
        }

        void beginStep() noexcept
        {

            if (!isKinematic)
            {
                previousPosition = position;
                previousOrientation = orientation;
            }
            posCorrectionVelocity = Vector3(0, 0, 0);
            posCorrectionAngular = Vector3(0, 0, 0);
            acceleration = Vector3(0, 0, 0);
            angularAcceleration = Vector3(0, 0, 0);

            if (!isStatic && !isSleeping)
                updateWorldInvInertia();
        }

        void update(float deltaTime) noexcept
        {
            if (isStatic || isSleeping || isKinematic)
                return;

            velocity += acceleration * deltaTime;
            angularVelocity += angularAcceleration * deltaTime;

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
                const float halfDt = 0.5f * deltaTime;
                const Quaternion omega(
                    angularVelocity.x * halfDt,
                    angularVelocity.y * halfDt,
                    angularVelocity.z * halfDt,
                    0.0f);
                const Quaternion deltaQ = omega * orientation;
                orientation.x += deltaQ.x;
                orientation.y += deltaQ.y;
                orientation.z += deltaQ.z;
                orientation.w += deltaQ.w;
                orientation.normalize();
                updateWorldInvInertia();
            }

            updateBoundsFromTransform();
        }

        void integratePseudoVelocity(float deltaTime) noexcept
        {
            if (isStatic || isSleeping || isKinematic)
                return;

            position += posCorrectionVelocity * deltaTime;

            const float angLenSq = posCorrectionAngular.lengthSquared();
            if (angLenSq > 1e-10f)
            {
                const float halfDt = 0.5f * deltaTime;
                const Quaternion omega(
                    posCorrectionAngular.x * halfDt,
                    posCorrectionAngular.y * halfDt,
                    posCorrectionAngular.z * halfDt,
                    0.0f);
                const Quaternion deltaQ = omega * orientation;
                orientation.x += deltaQ.x;
                orientation.y += deltaQ.y;
                orientation.z += deltaQ.z;
                orientation.w += deltaQ.w;
                orientation.normalize();
                updateWorldInvInertia();
            }

            updateBoundsFromTransform();
        }

        void interpolateTransforms(float alpha) noexcept
        {

            if (alpha < 0.0f)
                alpha = 0.0f;
            if (alpha > 1.0f)
                alpha = 1.0f;

            Vector3 rp;
            rp.x = previousPosition.x + (position.x - previousPosition.x) * alpha;
            rp.y = previousPosition.y + (position.y - previousPosition.y) * alpha;
            rp.z = previousPosition.z + (position.z - previousPosition.z) * alpha;

            float qx = previousOrientation.x + (orientation.x - previousOrientation.x) * alpha;
            float qy = previousOrientation.y + (orientation.y - previousOrientation.y) * alpha;
            float qz = previousOrientation.z + (orientation.z - previousOrientation.z) * alpha;
            float qw = previousOrientation.w + (orientation.w - previousOrientation.w) * alpha;

            const float dot = previousOrientation.x * orientation.x + previousOrientation.y * orientation.y + previousOrientation.z * orientation.z + previousOrientation.w * orientation.w;
            if (dot < 0.0f)
            {
                qx = previousOrientation.x + (-orientation.x - previousOrientation.x) * alpha;
                qy = previousOrientation.y + (-orientation.y - previousOrientation.y) * alpha;
                qz = previousOrientation.z + (-orientation.z - previousOrientation.z) * alpha;
                qw = previousOrientation.w + (-orientation.w - previousOrientation.w) * alpha;
            }

            Quaternion rr;
            rr.x = qx;
            rr.y = qy;
            rr.z = qz;
            rr.w = qw;
            rr.normalize();

            renderPos[0] = rp;
            renderRot[0] = rr;
            renderIdx = 0;
        }

        PIP3D_FORCE_INLINE void setPosition(const Vector3 &pos) noexcept
        {
            position = pos;
            previousPosition = pos;
            updateBoundsFromTransform();
            wakeUpInternal();
        }

        PIP3D_FORCE_INLINE void setOrientation(const Quaternion &q) noexcept
        {
            orientation = q;
            previousOrientation = q;
            updateWorldInvInertia();
            updateBoundsFromTransform();
            wakeUpInternal();
        }

        PIP3D_FORCE_INLINE void setStatic(bool s) noexcept
        {
            isStatic = s;
            if (isStatic)
            {
                isKinematic = false;
                isTrigger = false;
                velocity = Vector3(0, 0, 0);
                acceleration = Vector3(0, 0, 0);
                angularVelocity = Vector3(0, 0, 0);
                wakeUpInternal();
            }
            computeInertia();
            updateWorldInvInertia();
        }

        PIP3D_FORCE_INLINE void setKinematic(bool k) noexcept
        {
            if (k == isKinematic)
                return;
            isKinematic = k;
            if (isKinematic)
            {
                isStatic = false;
                canSleep = false;
                velocity = Vector3(0, 0, 0);
                acceleration = Vector3(0, 0, 0);
                angularVelocity = Vector3(0, 0, 0);
                savedMass_ = mass_;
                invMass = 0.0f;
                invInertia = Vector3(0.0f, 0.0f, 0.0f);
                for (int i = 0; i < 9; ++i)
                    worldInvInertia[i] = 0.0f;
                kinematicTargetPos = position;
                kinematicTargetRot = orientation;
                wakeUpInternal();
            }
            else
            {
                if (savedMass_ > 0.0f)
                {
                    mass_ = savedMass_;
                    invMass = 1.0f / savedMass_;
                }
                computeInertia();
                updateWorldInvInertia();
            }
        }

        PIP3D_FORCE_INLINE void setTrigger(bool t) noexcept { isTrigger = t; }

        PIP3D_FORCE_INLINE void wakeUp() noexcept
        {
            if (isSleeping)
                wakeUpInternal();
        }

        PIP3D_FORCE_INLINE void setCanSleep(bool value) noexcept
        {
            canSleep = value;
            if (!canSleep && isSleeping)
                wakeUpInternal();
        }

        PIP3D_FORCE_INLINE void setMass(float m) noexcept
        {
            if (m < 0.0f)
                m = 0.0f;
            mass_ = m;
            if (!isKinematic)
            {
                computeInertia();
                updateWorldInvInertia();
            }
            else
            {
                savedMass_ = m;
            }
        }

        PIP3D_FORCE_INLINE float getMass() const noexcept { return mass_; }
        PIP3D_FORCE_INLINE float getInvMass() const noexcept { return invMass; }

        PIP3D_FORCE_INLINE void setRestitution(float r) noexcept
        {
            if (r < 0.0f)
                r = 0.0f;
            if (r > 1.0f)
                r = 1.0f;
            restitution = r;
        }

        PIP3D_FORCE_INLINE void setFriction(float f) noexcept
        {
            if (f < 0.0f)
                f = 0.0f;
            if (f > 1.0f)
                f = 1.0f;
            friction = f;
        }

        PIP3D_FORCE_INLINE void setMaterial(const PhysicsMaterial &m) noexcept
        {
            setFriction(m.friction);
            setRestitution(m.restitution);
        }

        PIP3D_FORCE_INLINE bool isNan() const noexcept
        {
            return (position.x != position.x) ||
                   (position.y != position.y) ||
                   (position.z != position.z) ||
                   (velocity.x != velocity.x) ||
                   (velocity.y != velocity.y) ||
                   (velocity.z != velocity.z) ||
                   (angularVelocity.x != angularVelocity.x) ||
                   (angularVelocity.y != angularVelocity.y) ||
                   (angularVelocity.z != angularVelocity.z) ||
                   (orientation.x != orientation.x) ||
                   (orientation.y != orientation.y) ||
                   (orientation.z != orientation.z) ||
                   (orientation.w != orientation.w);
        }

        PIP3D_FORCE_INLINE void sanitize(const Vector3 &fallbackPos) noexcept
        {
            auto fixNan = [](float &v, float fallback)
            { if (v != v) v = fallback; };

            fixNan(position.x, fallbackPos.x);
            fixNan(position.y, fallbackPos.y);
            fixNan(position.z, fallbackPos.z);
            fixNan(previousPosition.x, position.x);
            fixNan(previousPosition.y, position.y);
            fixNan(previousPosition.z, position.z);
            fixNan(velocity.x, 0.0f);
            fixNan(velocity.y, 0.0f);
            fixNan(velocity.z, 0.0f);
            fixNan(angularVelocity.x, 0.0f);
            fixNan(angularVelocity.y, 0.0f);
            fixNan(angularVelocity.z, 0.0f);
            fixNan(acceleration.x, 0.0f);
            fixNan(acceleration.y, 0.0f);
            fixNan(acceleration.z, 0.0f);
            fixNan(posCorrectionVelocity.x, 0.0f);
            fixNan(posCorrectionVelocity.y, 0.0f);
            fixNan(posCorrectionVelocity.z, 0.0f);
            fixNan(posCorrectionAngular.x, 0.0f);
            fixNan(posCorrectionAngular.y, 0.0f);
            fixNan(posCorrectionAngular.z, 0.0f);
            fixNan(orientation.x, 0.0f);
            fixNan(orientation.y, 0.0f);
            fixNan(orientation.z, 0.0f);
            fixNan(orientation.w, 1.0f);
            fixNan(previousOrientation.x, orientation.x);
            fixNan(previousOrientation.y, orientation.y);
            fixNan(previousOrientation.z, orientation.z);
            fixNan(previousOrientation.w, orientation.w);

            const float qLenSq = orientation.x * orientation.x +
                                 orientation.y * orientation.y +
                                 orientation.z * orientation.z +
                                 orientation.w * orientation.w;
            if (qLenSq < 1e-10f)
                orientation = Quaternion();
            orientation.normalize();
            previousOrientation = orientation;

            isSleeping = false;
            sleepTimer = 0.0f;

            updateWorldInvInertia();
            updateBoundsFromTransform();
        }

    private:
        PIP3D_FORCE_INLINE void wakeUpInternal() noexcept
        {
            isSleeping = false;
            sleepTimer = 0.0f;
        }

        PIP3D_FORCE_INLINE void computeInertia() noexcept
        {
            if (isStatic || isKinematic || mass_ <= 0.0f)
            {
                invMass = 0.0f;
                invInertia = Vector3(0, 0, 0);
                return;
            }
            invMass = 1.0f / mass_;

            if (shape == BODY_SHAPE_BOX)
            {

                const float sx = size.x;
                const float sy = size.y;
                const float sz = size.z;

                const float ix = (mass_ / 12.0f) * (sy * sy + sz * sz);
                const float iy = (mass_ / 12.0f) * (sx * sx + sz * sz);
                const float iz = (mass_ / 12.0f) * (sx * sx + sy * sy);
                invInertia.x = ix > 0.0f ? 1.0f / ix : 0.0f;
                invInertia.y = iy > 0.0f ? 1.0f / iy : 0.0f;
                invInertia.z = iz > 0.0f ? 1.0f / iz : 0.0f;
            }
            else if (shape == BODY_SHAPE_CAPSULE)
            {
                const float r = radius;
                const float L = capsuleHalfHeight * 2.0f;
                const float r2 = r * r;

                const float V_cyl = kPi * r2 * L;
                const float V_sph = (4.0f / 3.0f) * kPi * r2 * r;
                const float V_tot = V_cyl + V_sph;
                const float m_cyl = (V_tot > 1e-12f) ? mass_ * (V_cyl / V_tot) : 0.0f;
                const float m_sph = (V_tot > 1e-12f) ? mass_ * (V_sph / V_tot) : 0.0f;

                const float I_cyl_trans = m_cyl * (r2 * 0.25f + L * L / 12.0f);
                const float I_sph_trans = (2.0f / 5.0f) * m_sph * r2 + m_sph * (L * 0.5f) * (L * 0.5f);
                const float I_trans = I_cyl_trans + I_sph_trans;

                const float I_cyl_axial = 0.5f * m_cyl * r2;
                const float I_sph_axial = (2.0f / 5.0f) * m_sph * r2;
                const float I_axial = I_cyl_axial + I_sph_axial;

                invInertia.x = I_trans > 0.0f ? 1.0f / I_trans : 0.0f;
                invInertia.y = I_axial > 0.0f ? 1.0f / I_axial : 0.0f;
                invInertia.z = I_trans > 0.0f ? 1.0f / I_trans : 0.0f;
            }
            else if (shape == BODY_SHAPE_CYLINDER)
            {

                const float r = radius;
                const float L = capsuleHalfHeight * 2.0f;
                const float r2 = r * r;
                const float I_axial = 0.5f * mass_ * r2;
                const float I_trans = mass_ * (3.0f * r2 + L * L) / 12.0f;
                invInertia.x = I_trans > 0.0f ? 1.0f / I_trans : 0.0f;
                invInertia.y = I_axial > 0.0f ? 1.0f / I_axial : 0.0f;
                invInertia.z = I_trans > 0.0f ? 1.0f / I_trans : 0.0f;
            }
            else if (shape == BODY_SHAPE_CONVEX)
            {
                Vector3 mn = convexVerts[0];
                Vector3 mx = convexVerts[0];
                for (int i = 1; i < convexCount; ++i)
                {
                    const Vector3 &v = convexVerts[i];
                    if (v.x < mn.x)
                        mn.x = v.x;
                    else if (v.x > mx.x)
                        mx.x = v.x;
                    if (v.y < mn.y)
                        mn.y = v.y;
                    else if (v.y > mx.y)
                        mx.y = v.y;
                    if (v.z < mn.z)
                        mn.z = v.z;
                    else if (v.z > mx.z)
                        mx.z = v.z;
                }
                const Vector3 extent = mx - mn;

                const float sx = extent.x;
                const float sy = extent.y;
                const float sz = extent.z;

                float ix = (mass_ / 12.0f) * (sy * sy + sz * sz);
                float iy = (mass_ / 12.0f) * (sx * sx + sz * sz);
                float iz = (mass_ / 12.0f) * (sx * sx + sy * sy);

                if (convexHullComplete && convexFaceCount >= 4)
                {
                    float volume = 0.0f;
                    float qxx = 0.0f;
                    float qyy = 0.0f;
                    float qzz = 0.0f;

                    for (int faceIdx = 0; faceIdx < convexFaceCount; ++faceIdx)
                    {
                        const ConvexFace &face = convexFaces[faceIdx];
                        if (face.vertCount < 3)
                            continue;
                        const Vector3 &a = convexVerts[face.vertIdx[0]];
                        for (int i = 1; i + 1 < face.vertCount; ++i)
                        {
                            const Vector3 &b = convexVerts[face.vertIdx[i]];
                            const Vector3 &c = convexVerts[face.vertIdx[i + 1]];
                            const float tetraVolume = a.dot(b.cross(c)) * (1.0f / 6.0f);
                            const float factor = tetraVolume * 0.1f;
                            qxx += factor * (a.x * a.x + b.x * b.x + c.x * c.x +
                                             a.x * b.x + a.x * c.x + b.x * c.x);
                            qyy += factor * (a.y * a.y + b.y * b.y + c.y * c.y +
                                             a.y * b.y + a.y * c.y + b.y * c.y);
                            qzz += factor * (a.z * a.z + b.z * b.z + c.z * c.z +
                                             a.z * b.z + a.z * c.z + b.z * c.z);
                            volume += tetraVolume;
                        }
                    }

                    if (volume < 0.0f)
                    {
                        volume = -volume;
                        qxx = -qxx;
                        qyy = -qyy;
                        qzz = -qzz;
                    }
                    if (volume > 1e-8f)
                    {
                        const float densityMass = mass_ / volume;
                        const float exactIx = densityMass * (qyy + qzz);
                        const float exactIy = densityMass * (qxx + qzz);
                        const float exactIz = densityMass * (qxx + qyy);
                        if (exactIx > 1e-8f && exactIy > 1e-8f && exactIz > 1e-8f)
                        {
                            ix = exactIx;
                            iy = exactIy;
                            iz = exactIz;
                        }
                    }
                }
                invInertia.x = ix > 0.0f ? 1.0f / ix : 0.0f;
                invInertia.y = iy > 0.0f ? 1.0f / iy : 0.0f;
                invInertia.z = iz > 0.0f ? 1.0f / iz : 0.0f;
            }
            else
            {

                const float i = 0.4f * mass_ * radius * radius;
                const float invI = i > 0.0f ? 1.0f / i : 0.0f;
                invInertia = Vector3(invI, invI, invI);
            }
        }
    };

}

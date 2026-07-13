#pragma once

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "../Dynamics/Body.hpp"

namespace pip3D
{
    class Rope
    {
    public:
        struct Node
        {
            Vector3 position;
            Vector3 prevPosition;
            bool fixed;

            Node()
                : position(0.0f, 0.0f, 0.0f),
                  prevPosition(0.0f, 0.0f, 0.0f),
                  fixed(false) {}
        };

        static constexpr int MAX_NODES = 64;

        void setIterations(int it)
        {
            if (it < 1)
                it = 1;
            if (it > 32)
                it = 32;
            iterations = it;
        }
        void setGravity(const Vector3 &g) { gravity = g; }
        void setAirDamping(float d)
        {
            if (d < 0.0f)
                d = 0.0f;
            if (d > 1.0f)
                d = 1.0f;
            airDamping = d;
        }
        void setFloorHeight(float h) { floorHeight = h; }
        void setFloorFriction(float f)
        {
            if (f < 0.0f)
                f = 0.0f;
            if (f > 1.0f)
                f = 1.0f;
            floorFriction = f;
        }
        void setCollisionFriction(float f)
        {
            if (f < 0.0f)
                f = 0.0f;
            if (f > 1.0f)
                f = 1.0f;
            collisionFriction = f;
        }

        int getNodeCount() const { return nodeCount; }

        const Node &getNode(int index) const
        {
            if (index < 0)
                index = 0;
            if (index >= nodeCount)
                index = nodeCount > 0 ? nodeCount - 1 : 0;
            return nodes[index];
        }
        Node &getNode(int index)
        {
            if (index < 0)
                index = 0;
            if (index >= nodeCount)
                index = nodeCount > 0 ? nodeCount - 1 : 0;
            return nodes[index];
        }

        Vector3 getEndDirection() const
        {
            if (nodeCount < 2)
                return Vector3(0.0f, -1.0f, 0.0f);
            const Node &a = nodes[nodeCount - 2];
            const Node &b = nodes[nodeCount - 1];
            Vector3 dir = b.position - a.position;
            dir.normalize();
            return dir;
        }

        void initLinear(const Vector3 &start,
                        const Vector3 &end,
                        int segments,
                        bool fixStart = true,
                        bool fixEnd = false)
        {
            if (segments < 1)
                segments = 1;
            if (segments >= MAX_NODES)
                segments = MAX_NODES - 1;

            nodeCount = segments + 1;

            Vector3 delta = end - start;
            float totalLen = delta.length();
            if (totalLen <= 1e-5f)
            {
                delta = Vector3(0.0f, -1.0f, 0.0f);
                totalLen = 1.0f;
            }

            segmentLength = totalLen / static_cast<float>(segments);
            Vector3 step = delta * (1.0f / static_cast<float>(segments));

            for (int i = 0; i < nodeCount; ++i)
            {
                Vector3 p = start + step * static_cast<float>(i);
                nodes[i].position = p;
                nodes[i].prevPosition = p;
                nodes[i].fixed = false;
            }

            if (fixStart)
                nodes[0].fixed = true;
            if (fixEnd)
                nodes[nodeCount - 1].fixed = true;
        }

        void simulate(float dt)
        {
            if (nodeCount <= 1)
                return;
            if (dt <= 0.0f)
                return;

            float dt2 = dt * dt;

            for (int i = 0; i < nodeCount; ++i)
            {
                Node &n = nodes[i];
                if (n.fixed)
                    continue;

                Vector3 cur = n.position;
                Vector3 vel = (n.position - n.prevPosition) * airDamping;
                Vector3 next = cur + vel + gravity * dt2;

                n.prevPosition = cur;
                n.position = next;
            }

            for (int it = 0; it < iterations; ++it)
                satisfyConstraints();
        }

        void resolveCollisions(RigidBody **bodies, size_t bodyCount)
        {
            if (nodeCount == 0)
                return;

            for (int i = 0; i < nodeCount; ++i)
            {
                Node &n = nodes[i];
                if (n.fixed)
                    continue;

                if (n.position.y < floorHeight)
                {
                    n.position.y = floorHeight;
                    applyFriction(n, Vector3(0.0f, 1.0f, 0.0f), floorFriction);
                }

                for (size_t b = 0; b < bodyCount; ++b)
                {
                    RigidBody *body = bodies[b];
                    if (!body)
                        continue;

                    if (body->shape == BODY_SHAPE_SPHERE)
                        resolveSphereCollision(n, *body);
                    else
                        resolveBoxCollision(n, *body);
                }
            }
        }

        Rope()
            : nodeCount(0),
              segmentLength(0.25f),
              iterations(8),
              gravity(0.0f, -9.81f, 0.0f),
              airDamping(0.98f),
              floorHeight(0.0f),
              floorFriction(0.6f),
              collisionFriction(0.6f)
        {
        }

    private:
        Node nodes[MAX_NODES];
        int nodeCount;

        float segmentLength;
        int iterations;

        Vector3 gravity;
        float airDamping;

        float floorHeight;
        float floorFriction;
        float collisionFriction;

        void satisfyConstraints()
        {
            for (int i = 0; i < nodeCount - 1; ++i)
            {
                Node &a = nodes[i];
                Node &b = nodes[i + 1];

                Vector3 delta = b.position - a.position;
                float distSq = delta.lengthSquared();
                if (distSq <= 1e-8f)
                    continue;

                float dist = sqrtf(distSq);
                float diff = (dist - segmentLength) / dist;

                if (!a.fixed && !b.fixed)
                {
                    Vector3 correction = delta * (0.5f * diff);
                    a.position += correction;
                    b.position -= correction;
                }
                else if (a.fixed && !b.fixed)
                {
                    b.position -= delta * diff;
                }
                else if (!a.fixed && b.fixed)
                {
                    a.position += delta * diff;
                }
            }
        }

        void applyFriction(Node &n, const Vector3 &, float friction)
        {
            if (friction <= 0.0f)
                return;
            if (friction > 1.0f)
                friction = 1.0f;

            Vector3 vel = n.position - n.prevPosition;
            vel *= (1.0f - friction);
            n.prevPosition = n.position - vel;
        }

        void resolveSphereCollision(Node &n, const RigidBody &body)
        {
            float r = body.radius;
            if (r <= 0.0f)
                return;

            Vector3 toNode = n.position - body.position;
            float distSq = toNode.lengthSquared();
            float rSq = r * r;
            if (distSq >= rSq || distSq <= 1e-8f)
                return;

            float dist = sqrtf(distSq);
            Vector3 normal = toNode * (1.0f / dist);
            float penetration = r - dist;

            n.position += normal * penetration;
            applyFriction(n, normal, collisionFriction);
        }

        void resolveBoxCollision(Node &n, const RigidBody &body)
        {
            Vector3 half = body.size * 0.5f;
            if (half.x <= 0.0f || half.y <= 0.0f || half.z <= 0.0f)
                return;

            Quaternion invRot = body.orientation.conjugate();
            Vector3 local = invRot.rotate(n.position - body.position);

            float ax = fabsf(local.x);
            float ay = fabsf(local.y);
            float az = fabsf(local.z);

            if (ax > half.x || ay > half.y || az > half.z)
                return;

            float dx = half.x - ax;
            float dy = half.y - ay;
            float dz = half.z - az;

            Vector3 localNormal(0.0f, 0.0f, 0.0f);
            float penetration = dx;

            if (dx < dy && dx < dz)
            {
                localNormal.x = (local.x > 0.0f) ? 1.0f : -1.0f;
                penetration = dx;
            }
            else if (dy < dz)
            {
                localNormal.y = (local.y > 0.0f) ? 1.0f : -1.0f;
                penetration = dy;
            }
            else
            {
                localNormal.z = (local.z > 0.0f) ? 1.0f : -1.0f;
                penetration = dz;
            }

            Vector3 worldNormal = body.orientation.rotate(localNormal);

            n.position += worldNormal * penetration;
            applyFriction(n, worldNormal, collisionFriction);
        }
    };
}

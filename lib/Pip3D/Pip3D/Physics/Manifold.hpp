#pragma once

#include <cmath>

#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Math/Collision.hpp"
#include "Physics/Types.hpp"
#include "Physics/RigidBody/Body.hpp"
#include "Physics/RigidBody/Contacts.hpp"

namespace pip3D
{

    class ManifoldPool
    {
    public:
        static constexpr int kMaxManifolds = PhysicsConfig::MAX_MANIFOLDS;

    private:
        ContactManifold manifolds_[kMaxManifolds];
        bool active_[kMaxManifolds];
        int count_;

        PIP3D_FORCE_INLINE static void buildTangentBasis(const Vector3 &n,
                                                         Vector3 &t1,
                                                         Vector3 &t2) noexcept
        {
            if (fabsf(n.x) <= 0.70710678f)
                t1 = Vector3(1.0f - n.x * n.x, -n.y * n.x, -n.z * n.x);
            else
                t1 = Vector3(-n.x * n.y, 1.0f - n.y * n.y, -n.z * n.y);
            t1.normalize();
            t2 = n.cross(t1);
        }

    public:
        ManifoldPool() : count_(0)
        {
            for (int i = 0; i < kMaxManifolds; ++i)
            {
                active_[i] = false;
                manifolds_[i].hasCollision = false;
                manifolds_[i].contactCount = 0;
                manifolds_[i].bodyA = nullptr;
                manifolds_[i].bodyB = nullptr;
            }
        }

        void clear() noexcept
        {
            for (int i = 0; i < kMaxManifolds; ++i)
            {
                active_[i] = false;
                manifolds_[i].hasCollision = false;
                manifolds_[i].contactCount = 0;
                manifolds_[i].bodyA = nullptr;
                manifolds_[i].bodyB = nullptr;
            }
            count_ = 0;
        }

        int getCount() const { return count_; }

        ContactManifold &getManifold(int i) { return manifolds_[i]; }
        const ContactManifold &getManifold(int i) const { return manifolds_[i]; }

        bool isActive(int i) const { return active_[i]; }

        ContactManifold *find(RigidBody *a, RigidBody *b) noexcept
        {
            for (int i = 0; i < kMaxManifolds; ++i)
            {
                if (!active_[i])
                    continue;
                ContactManifold &m = manifolds_[i];
                if ((m.bodyA == a && m.bodyB == b) || (m.bodyA == b && m.bodyB == a))
                    return &m;
            }
            return nullptr;
        }

        ContactManifold *findOrCreate(RigidBody *a, RigidBody *b) noexcept
        {
            ContactManifold *existing = find(a, b);
            if (existing)
            {
                if (existing->bodyA != a || existing->bodyB != b)
                {

                    for (int i = 0; i < existing->contactCount; ++i)
                    {
                        Contact &c = existing->contacts[i];
                        const Vector3 localA = c.localPointA;
                        c.localPointA = c.localPointB;
                        c.localPointB = localA;
                        c.accumulatedImpulse = 0.0f;
                        c.tangentImpulse1 = 0.0f;
                        c.tangentImpulse2 = 0.0f;
                    }
                    existing->bodyA = a;
                    existing->bodyB = b;
                    existing->normal = -existing->normal;
                }
                return existing;
            }

            for (int i = 0; i < kMaxManifolds; ++i)
            {
                if (!active_[i])
                {
                    ContactManifold &m = manifolds_[i];
                    m.bodyA = a;
                    m.bodyB = b;
                    m.normal = Vector3(0, 1, 0);
                    m.contactCount = 0;
                    m.hasCollision = false;
                    m.hasRealContact = false;
                    for (int j = 0; j < PhysicsConfig::MAX_CONTACT_POINTS; ++j)
                    {
                        m.contacts[j].accumulatedImpulse = 0.0f;
                        m.contacts[j].tangentImpulse1 = 0.0f;
                        m.contacts[j].tangentImpulse2 = 0.0f;
                        m.contacts[j].lifetime = 0;
                        m.contacts[j].featureId = 0;
                    }
                    active_[i] = true;
                    ++count_;
                    return &m;
                }
            }
            return nullptr;
        }

        void deactivate(int idx) noexcept
        {
            if (idx < 0 || idx >= kMaxManifolds)
                return;
            if (active_[idx])
            {
                active_[idx] = false;
                manifolds_[idx].contactCount = 0;
                manifolds_[idx].hasCollision = false;
                manifolds_[idx].bodyA = nullptr;
                manifolds_[idx].bodyB = nullptr;
                --count_;
            }
        }

        void removeBody(const RigidBody *body) noexcept
        {
            if (!body)
                return;
            for (int i = 0; i < kMaxManifolds; ++i)
            {
                if (!active_[i])
                    continue;
                const ContactManifold &m = manifolds_[i];
                if (m.bodyA == body || m.bodyB == body)
                    deactivate(i);
            }
        }

        void refreshManifold(ContactManifold &m) noexcept
        {
            if (!m.bodyA || !m.bodyB)
                return;

            RigidBody *a = m.bodyA;
            RigidBody *b = m.bodyB;
            const Vector3 &n = m.normal;

            for (int i = m.contactCount - 1; i >= 0; --i)
            {
                Contact &c = m.contacts[i];

                c.pos = a->position + a->orientation.rotate(c.localPointA);
                Vector3 posB = b->position + b->orientation.rotate(c.localPointB);

                float separation = (posB - c.pos).dot(n);
                c.penetration = -separation;

                if (separation > PhysicsConfig::CONTACT_BREAKING_THRESHOLD)
                {
                    removePoint(m, i);
                    continue;
                }

                Vector3 projectedA = c.pos + n * separation;
                Vector3 diff = posB - projectedA;
                float driftSq = diff.lengthSquared();
                if (driftSq > PhysicsConfig::CONTACT_BREAKING_THRESHOLD * PhysicsConfig::CONTACT_BREAKING_THRESHOLD)
                {
                    removePoint(m, i);
                    continue;
                }

                c.pos = c.pos + n * (separation * 0.5f);

                if (c.lifetime < 0xFFFF)
                    ++c.lifetime;
            }
        }

        void mergeContacts(ContactManifold &m, const ContactManifold &fresh) noexcept
        {
            if (!m.bodyA || !m.bodyB)
                return;

            RigidBody *a = m.bodyA;
            RigidBody *b = m.bodyB;

            bool matched[PhysicsConfig::MAX_CONTACT_POINTS] = {false, false, false, false};

            for (int fi = 0; fi < fresh.contactCount && fi < PhysicsConfig::MAX_CONTACT_POINTS; ++fi)
            {
                const Contact &fc = fresh.contacts[fi];

                Vector3 localA = a->orientation.conjugate().rotate(fc.pos - a->position);
                Vector3 localB = b->orientation.conjugate().rotate(fc.pos - b->position);

                int bestIdx = -1;
                float bestDistSq = PhysicsConfig::CONTACT_MATCH_DISTANCE_SQ;

                for (int ei = 0; ei < m.contactCount; ++ei)
                {
                    if (matched[ei])
                        continue;

                    if (fc.featureId != 0 && m.contacts[ei].featureId == fc.featureId)
                    {
                        bestIdx = ei;
                        break;
                    }

                    float distSq = FLT_MAX;
                    if (!a->isStatic)
                    {
                        Vector3 diffA = m.contacts[ei].localPointA - localA;
                        distSq = diffA.lengthSquared();
                    }
                    if (!b->isStatic)
                    {
                        Vector3 diffB = m.contacts[ei].localPointB - localB;
                        float distSqB = diffB.lengthSquared();
                        if (distSqB < distSq)
                            distSq = distSqB;
                    }

                    if (distSq < bestDistSq)
                    {
                        bestDistSq = distSq;
                        bestIdx = ei;
                    }
                }

                if (bestIdx >= 0)
                {
                    Contact &ec = m.contacts[bestIdx];
                    ec.pos = fc.pos;
                    ec.localPointA = localA;
                    ec.localPointB = localB;
                    ec.penetration = fc.penetration;
                    ec.featureId = fc.featureId;
                    ec.material = fc.material;

                    matched[bestIdx] = true;
                }
                else
                {
                    int replaceIdx = -1;
                    if (m.contactCount < PhysicsConfig::MAX_CONTACT_POINTS)
                    {
                        replaceIdx = m.contactCount;
                    }
                    else
                    {
                        float minPen = FLT_MAX;
                        for (int ei = 0; ei < m.contactCount; ++ei)
                        {
                            if (matched[ei])
                                continue;
                            if (m.contacts[ei].penetration < minPen)
                            {
                                minPen = m.contacts[ei].penetration;
                                replaceIdx = ei;
                            }
                        }
                    }

                    if (replaceIdx >= 0)
                    {
                        Contact &nc = m.contacts[replaceIdx];
                        nc.pos = fc.pos;
                        nc.localPointA = localA;
                        nc.localPointB = localB;
                        nc.penetration = fc.penetration;
                        nc.featureId = fc.featureId;
                        nc.material = fc.material;
                        nc.accumulatedImpulse = 0.0f;
                        nc.tangentImpulse1 = 0.0f;
                        nc.tangentImpulse2 = 0.0f;
                        nc.lifetime = 0;
                        matched[replaceIdx] = true;
                        if (replaceIdx == m.contactCount)
                            ++m.contactCount;
                    }
                }
            }

            for (int i = m.contactCount - 1; i >= 0; --i)
            {
                if (!matched[i])
                    removePoint(m, i);
            }

            if (fresh.contactCount > 0)
            {
                const float normalAlignment = m.normal.dot(fresh.normal);
                if (normalAlignment > 0.95f)
                {

                    Vector3 oldT1, oldT2, newT1, newT2;
                    buildTangentBasis(m.normal, oldT1, oldT2);
                    buildTangentBasis(fresh.normal, newT1, newT2);
                    for (int i = 0; i < m.contactCount; ++i)
                    {
                        Contact &c = m.contacts[i];
                        const Vector3 tangentImpulse = oldT1 * c.tangentImpulse1 +
                                                       oldT2 * c.tangentImpulse2;
                        c.tangentImpulse1 = tangentImpulse.dot(newT1);
                        c.tangentImpulse2 = tangentImpulse.dot(newT2);
                    }
                }
                else
                {

                    for (int i = 0; i < m.contactCount; ++i)
                    {
                        m.contacts[i].accumulatedImpulse = 0.0f;
                        m.contacts[i].tangentImpulse1 = 0.0f;
                        m.contacts[i].tangentImpulse2 = 0.0f;
                    }
                }
                m.normal = fresh.normal;
            }

            m.hasCollision = (m.contactCount > 0);
            m.hasRealContact = false;
            for (int i = 0; i < m.contactCount; ++i)
            {
                if (m.contacts[i].penetration > 0.0f)
                {
                    m.hasRealContact = true;
                    break;
                }
            }
        }

        void removePoint(ContactManifold &m, int idx) noexcept
        {
            if (idx < 0 || idx >= m.contactCount)
                return;

            if (idx < m.contactCount - 1)
            {
                m.contacts[idx] = m.contacts[m.contactCount - 1];
            }
            --m.contactCount;
        }

        void pruneEmpty() noexcept
        {
            for (int i = 0; i < kMaxManifolds; ++i)
            {
                if (!active_[i])
                    continue;
                if (manifolds_[i].contactCount == 0)
                {
                    deactivate(i);
                }
            }
        }
    };

}

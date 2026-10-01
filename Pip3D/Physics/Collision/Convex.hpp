#pragma once

#include <float.h>
#include <math.h>

#include "Helpers.hpp"
#include "GJK.hpp"
#include "Simplex.hpp"
#include "EPA.hpp"

namespace pip3D
{

    struct ClipFace
    {
        Vector3 normal;
        float offset;
        Vector3 verts[12];
        int vertCount;
    };

    PIP3D_FORCE_INLINE ClipFace
    makeClipFaceFromConvex(const RigidBody *body, int faceIdx) noexcept
    {
        ClipFace f;
        const ConvexFace &cf = body->convexFaces[faceIdx];
        f.normal = body->orientation.rotate(cf.normal);
        f.offset = cf.offset + f.normal.dot(body->position);
        f.vertCount = cf.vertCount;
        const int n = (cf.vertCount <= 12) ? cf.vertCount : 12;
        for (int i = 0; i < n; ++i)
        {
            const Vector3 lv = body->convexVerts[cf.vertIdx[i]];
            f.verts[i] = body->position + body->orientation.rotate(lv);
        }
        for (int i = n; i < 12; ++i)
            f.verts[i] = f.verts[0];
        return f;
    }

    PIP3D_FORCE_INLINE ClipFace
    makeClipFaceFromBox(const RigidBody *box, int faceIdx) noexcept
    {
        ClipFace f;
        const Vector3 half = box->size * 0.5f;

        static const Vector3 localNormals[6] = {
            Vector3(1, 0, 0), Vector3(-1, 0, 0),
            Vector3(0, 1, 0), Vector3(0, -1, 0),
            Vector3(0, 0, 1), Vector3(0, 0, -1)};

        static const Vector3 localCorners[6][4] = {
            {Vector3(1, 1, -1), Vector3(1, 1, 1), Vector3(1, -1, 1), Vector3(1, -1, -1)},
            {Vector3(-1, 1, 1), Vector3(-1, 1, -1), Vector3(-1, -1, -1), Vector3(-1, -1, 1)},
            {Vector3(-1, 1, 1), Vector3(1, 1, 1), Vector3(1, 1, -1), Vector3(-1, 1, -1)},
            {Vector3(-1, -1, -1), Vector3(1, -1, -1), Vector3(1, -1, 1), Vector3(-1, -1, 1)},
            {Vector3(1, 1, 1), Vector3(-1, 1, 1), Vector3(-1, -1, 1), Vector3(1, -1, 1)},
            {Vector3(-1, 1, -1), Vector3(1, 1, -1), Vector3(1, -1, -1), Vector3(-1, -1, -1)}};

        f.normal = box->orientation.rotate(localNormals[faceIdx]);
        f.vertCount = 4;
        for (int i = 0; i < 4; ++i)
        {
            const Vector3 &lc = localCorners[faceIdx][i];
            const Vector3 scaled(lc.x * half.x, lc.y * half.y, lc.z * half.z);
            f.verts[i] = box->position + box->orientation.rotate(scaled);
        }
        f.offset = f.normal.dot(f.verts[0]);
        return f;
    }

    PIP3D_FORCE_INLINE Vector3
    boxFaceNormalWorld(const RigidBody *box, int faceIdx) noexcept
    {
        static const Vector3 localNormals[6] = {
            Vector3(1, 0, 0), Vector3(-1, 0, 0),
            Vector3(0, 1, 0), Vector3(0, -1, 0),
            Vector3(0, 0, 1), Vector3(0, 0, -1)};
        return box->orientation.rotate(localNormals[faceIdx]);
    }

    PIP3D_FORCE_INLINE int
    findBestBoxFaceAlignment(const RigidBody *box, const Vector3 &nWorld) noexcept
    {
        static const Vector3 localNormals[6] = {
            Vector3(1, 0, 0), Vector3(-1, 0, 0),
            Vector3(0, 1, 0), Vector3(0, -1, 0),
            Vector3(0, 0, 1), Vector3(0, 0, -1)};

        int best = 0;
        float bestDot = -FLT_MAX;
        for (int i = 0; i < 6; ++i)
        {
            const Vector3 wn = box->orientation.rotate(localNormals[i]);
            const float d = wn.dot(nWorld);
            if (d > bestDot)
            {
                bestDot = d;
                best = i;
            }
        }
        return best;
    }

    PIP3D_FORCE_INLINE int
    findBestConvexFaceAlignment(const RigidBody *body, const Vector3 &nWorld) noexcept
    {
        int best = -1;
        float bestDot = -FLT_MAX;
        for (int i = 0; i < body->convexFaceCount; ++i)
        {
            const Vector3 wn = body->orientation.rotate(body->convexFaces[i].normal);
            const float d = wn.dot(nWorld);
            if (d > bestDot)
            {
                bestDot = d;
                best = i;
            }
        }
        return best;
    }

    PIP3D_FORCE_INLINE bool
    isPolyhedral(const RigidBody *body) noexcept
    {
        return body->shape == BODY_SHAPE_BOX ||
               ((body->shape == BODY_SHAPE_CONVEX || body->shape == BODY_SHAPE_CYLINDER) &&
                body->convexHullComplete && body->convexFaceCount >= 4);
    }

    inline void collectPolyEdgeDirections(const RigidBody *body,
                                          Vector3 *outDirections,
                                          int &outCount,
                                          int maxDirections) noexcept
    {
        outCount = 0;
        if (body->shape == BODY_SHAPE_BOX)
        {
            outDirections[outCount++] = body->orientation.rotate(Vector3(1.0f, 0.0f, 0.0f));
            outDirections[outCount++] = body->orientation.rotate(Vector3(0.0f, 1.0f, 0.0f));
            outDirections[outCount++] = body->orientation.rotate(Vector3(0.0f, 0.0f, 1.0f));
            return;
        }

        for (int faceIdx = 0; faceIdx < body->convexFaceCount && outCount < maxDirections; ++faceIdx)
        {
            const ConvexFace &face = body->convexFaces[faceIdx];
            for (int i = 0; i < face.vertCount && outCount < maxDirections; ++i)
            {
                const int j = (i + 1) % face.vertCount;
                const Vector3 localEdge = body->convexVerts[face.vertIdx[j]] -
                                          body->convexVerts[face.vertIdx[i]];
                const float edgeLenSq = localEdge.lengthSquared();
                if (edgeLenSq < 1e-12f)
                    continue;

                const Vector3 edge = body->orientation.rotate(
                    localEdge * FastMath::fastInvSqrt(edgeLenSq));
                bool duplicateDirection = false;
                for (int k = 0; k < outCount; ++k)
                {
                    if (fabsf(outDirections[k].dot(edge)) > 0.9999f)
                    {
                        duplicateDirection = true;
                        break;
                    }
                }
                if (!duplicateDirection)
                    outDirections[outCount++] = edge;
            }
        }
    }

    inline bool findPolyhedralPenetrationAxis(const RigidBody *a, const RigidBody *b,
                                              Vector3 &outNormal, float &outDepth) noexcept
    {
        constexpr int kMaxEdgeDirections = 64;
        const Vector3 centerDelta = b->position - a->position;
        float minOverlap = FLT_MAX;
        Vector3 bestAxis(0.0f, 1.0f, 0.0f);

        auto testAxis = [&](const Vector3 &axisCandidate) -> bool
        {
            const float axisLenSq = axisCandidate.lengthSquared();
            if (axisLenSq < 1e-12f)
                return true;
            const Vector3 axis = axisCandidate * FastMath::fastInvSqrt(axisLenSq);
            const float minA = a->support(-axis).dot(axis);
            const float maxA = a->support(axis).dot(axis);
            const float minB = b->support(-axis).dot(axis);
            const float maxB = b->support(axis).dot(axis);
            const float overlap = (maxA < maxB ? maxA : maxB) -
                                  (minA > minB ? minA : minB);
            if (overlap < -PhysicsConfig::CONTACT_SKIN)
                return false;
            if (overlap < minOverlap)
            {
                minOverlap = overlap;
                bestAxis = (centerDelta.dot(axis) >= 0.0f) ? axis : -axis;
            }
            return true;
        };

        const int faceCountA = (a->shape == BODY_SHAPE_BOX) ? 6 : a->convexFaceCount;
        const int faceCountB = (b->shape == BODY_SHAPE_BOX) ? 6 : b->convexFaceCount;
        for (int i = 0; i < faceCountA; ++i)
        {
            const Vector3 n = (a->shape == BODY_SHAPE_BOX)
                                  ? boxFaceNormalWorld(a, i)
                                  : a->orientation.rotate(a->convexFaces[i].normal);
            if (!testAxis(n))
                return false;
        }
        for (int i = 0; i < faceCountB; ++i)
        {
            const Vector3 n = (b->shape == BODY_SHAPE_BOX)
                                  ? boxFaceNormalWorld(b, i)
                                  : b->orientation.rotate(b->convexFaces[i].normal);
            if (!testAxis(n))
                return false;
        }

        Vector3 edgesA[kMaxEdgeDirections];
        Vector3 edgesB[kMaxEdgeDirections];
        int edgeCountA = 0;
        int edgeCountB = 0;
        collectPolyEdgeDirections(a, edgesA, edgeCountA, kMaxEdgeDirections);
        collectPolyEdgeDirections(b, edgesB, edgeCountB, kMaxEdgeDirections);
        for (int i = 0; i < edgeCountA; ++i)
        {
            for (int j = 0; j < edgeCountB; ++j)
            {
                if (!testAxis(edgesA[i].cross(edgesB[j])))
                    return false;
            }
        }

        if (minOverlap == FLT_MAX)
            return false;
        outNormal = bestAxis;
        outDepth = minOverlap > 0.0f ? minOverlap : 0.0f;
        return true;
    }

    PIP3D_FORCE_INLINE Vector3
    closestPointOnTriangle(const Vector3 &p, const Vector3 &a,
                           const Vector3 &b, const Vector3 &c) noexcept
    {
        const Vector3 ab = b - a;
        const Vector3 ac = c - a;
        if (ab.cross(ac).lengthSquared() < 1e-12f)
        {
            auto closestOnSegment = [&](const Vector3 &start, const Vector3 &end) noexcept
            {
                const Vector3 edge = end - start;
                const float edgeLenSq = edge.lengthSquared();
                if (edgeLenSq < 1e-12f)
                    return start;
                float t = (p - start).dot(edge) / edgeLenSq;
                t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
                return start + edge * t;
            };

            Vector3 closest = closestOnSegment(a, b);
            float closestDistSq = (p - closest).lengthSquared();
            const Vector3 bc = closestOnSegment(b, c);
            const float bcDistSq = (p - bc).lengthSquared();
            if (bcDistSq < closestDistSq)
            {
                closest = bc;
                closestDistSq = bcDistSq;
            }
            const Vector3 ca = closestOnSegment(c, a);
            if ((p - ca).lengthSquared() < closestDistSq)
                closest = ca;
            return closest;
        }
        const Vector3 ap = p - a;
        const float d1 = ab.dot(ap);
        const float d2 = ac.dot(ap);
        if (d1 <= 0.0f && d2 <= 0.0f)
            return a;

        const Vector3 bp = p - b;
        const float d3 = ab.dot(bp);
        const float d4 = ac.dot(bp);
        if (d3 >= 0.0f && d4 <= d3)
            return b;

        const float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
            return a + ab * (d1 / (d1 - d3));

        const Vector3 cp = p - c;
        const float d5 = ab.dot(cp);
        const float d6 = ac.dot(cp);
        if (d6 >= 0.0f && d5 <= d6)
            return c;

        const float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
            return a + ac * (d2 / (d2 - d6));

        const float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
            return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));

        const float invDenom = 1.0f / (va + vb + vc);
        return a + ab * (vb * invDenom) + ac * (vc * invDenom);
    }

    inline ContactManifold detectPolyhedronSphere(RigidBody *polyhedron,
                                                  RigidBody *sphere) noexcept
    {
        ContactManifold info;
        info.hasCollision = false;
        info.hasRealContact = false;
        info.bodyA = polyhedron;
        info.bodyB = sphere;
        info.contactCount = 0;
        info.normal = Vector3(0.0f, 1.0f, 0.0f);
        for (int i = 0; i < PhysicsConfig::MAX_CONTACT_POINTS; ++i)
        {
            info.contacts[i].pos = Vector3(0.0f, 0.0f, 0.0f);
            info.contacts[i].localPointA = Vector3(0.0f, 0.0f, 0.0f);
            info.contacts[i].localPointB = Vector3(0.0f, 0.0f, 0.0f);
            info.contacts[i].penetration = 0.0f;
            info.contacts[i].featureId = 0;
            info.contacts[i].lifetime = 0;
        }

        const Vector3 localCenter = polyhedron->orientation.conjugate().rotate(
            sphere->position - polyhedron->position);
        float closestDistSq = FLT_MAX;
        Vector3 closestLocal(0.0f, 0.0f, 0.0f);
        Vector3 closestNormal(0.0f, 1.0f, 0.0f);
        int closestFace = 0;
        bool centerInside = true;

        const int faceCount = (polyhedron->shape == BODY_SHAPE_BOX) ? 6 : polyhedron->convexFaceCount;
        for (int faceIdx = 0; faceIdx < faceCount; ++faceIdx)
        {
            const ClipFace face = (polyhedron->shape == BODY_SHAPE_BOX)
                                      ? makeClipFaceFromBox(polyhedron, faceIdx)
                                      : makeClipFaceFromConvex(polyhedron, faceIdx);
            const Vector3 localNormal = polyhedron->orientation.conjugate().rotate(face.normal);
            const float localOffset = localNormal.dot(
                polyhedron->orientation.conjugate().rotate(face.verts[0] - polyhedron->position));
            if (localNormal.dot(localCenter) - localOffset > PhysicsConfig::CONTACT_SKIN)
                centerInside = false;

            const Vector3 a = polyhedron->orientation.conjugate().rotate(face.verts[0] - polyhedron->position);
            for (int i = 1; i + 1 < face.vertCount; ++i)
            {
                const Vector3 b = polyhedron->orientation.conjugate().rotate(face.verts[i] - polyhedron->position);
                const Vector3 c = polyhedron->orientation.conjugate().rotate(face.verts[i + 1] - polyhedron->position);
                const Vector3 candidate = closestPointOnTriangle(localCenter, a, b, c);
                const float distSq = (localCenter - candidate).lengthSquared();
                if (distSq < closestDistSq)
                {
                    closestDistSq = distSq;
                    closestLocal = candidate;
                    closestNormal = localNormal;
                    closestFace = faceIdx;
                }
            }
        }

        if (closestDistSq == FLT_MAX)
            return info;

        const float distance = sqrtf(closestDistSq);
        if (!centerInside && distance > sphere->radius + PhysicsConfig::CONTACT_SKIN)
            return info;

        Vector3 localNormal = centerInside ? (closestLocal - localCenter)
                                           : (localCenter - closestLocal);
        const float normalLenSq = localNormal.lengthSquared();
        if (normalLenSq > 1e-12f)
            localNormal *= FastMath::fastInvSqrt(normalLenSq);
        else if (centerInside)
            localNormal = closestNormal;
        else
            localNormal = -closestNormal;

        info.hasCollision = true;
        info.contactCount = 1;
        info.normal = polyhedron->orientation.rotate(localNormal);
        info.contacts[0].pos = polyhedron->position + polyhedron->orientation.rotate(closestLocal);
        info.contacts[0].penetration = centerInside ? sphere->radius + distance
                                                    : (sphere->radius - distance > 0.0f
                                                           ? sphere->radius - distance
                                                           : 0.0f);
        info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_FACE,
                                                   static_cast<uint32_t>(closestFace));
        return info;
    }

    PIP3D_FORCE_INLINE bool
    segmentIntersectsTriangle(const Vector3 &p0, const Vector3 &p1,
                              const Vector3 &a, const Vector3 &b, const Vector3 &c,
                              Vector3 &outPoint) noexcept
    {
        const Vector3 dir = p1 - p0;
        const Vector3 edgeAB = b - a;
        const Vector3 edgeAC = c - a;
        const Vector3 h = dir.cross(edgeAC);
        const float det = edgeAB.dot(h);
        if (fabsf(det) < 1e-8f)
            return false;

        const float invDet = 1.0f / det;
        const Vector3 s = p0 - a;
        const float u = s.dot(h) * invDet;
        if (u < 0.0f || u > 1.0f)
            return false;
        const Vector3 q = s.cross(edgeAB);
        const float v = dir.dot(q) * invDet;
        if (v < 0.0f || u + v > 1.0f)
            return false;
        const float t = edgeAC.dot(q) * invDet;
        if (t < 0.0f || t > 1.0f)
            return false;

        outPoint = p0 + dir * t;
        return true;
    }

    inline void closestPointsOnSegments(const Vector3 &p0, const Vector3 &p1,
                                        const Vector3 &q0, const Vector3 &q1,
                                        Vector3 &outP, Vector3 &outQ) noexcept
    {
        const Vector3 d1 = p1 - p0;
        const Vector3 d2 = q1 - q0;
        const Vector3 r = p0 - q0;
        const float a = d1.dot(d1);
        const float e = d2.dot(d2);
        const float f = d2.dot(r);
        float s = 0.0f;
        float t = 0.0f;

        if (a <= 1e-12f && e <= 1e-12f)
        {
            outP = p0;
            outQ = q0;
            return;
        }
        if (a <= 1e-12f)
        {
            t = f / e;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        }
        else
        {
            const float c = d1.dot(r);
            if (e <= 1e-12f)
            {
                s = -c / a;
                s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
            }
            else
            {
                const float b = d1.dot(d2);
                const float denom = a * e - b * b;
                if (fabsf(denom) > 1e-12f)
                    s = (b * f - c * e) / denom;
                s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
                t = (b * s + f) / e;
                if (t < 0.0f)
                {
                    t = 0.0f;
                    s = -c / a;
                    s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
                }
                else if (t > 1.0f)
                {
                    t = 1.0f;
                    s = (b - c) / a;
                    s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
                }
            }
        }

        outP = p0 + d1 * s;
        outQ = q0 + d2 * t;
    }

    inline ContactManifold detectPolyhedronCapsule(RigidBody *polyhedron,
                                                   RigidBody *capsule) noexcept
    {
        ContactManifold info;
        info.hasCollision = false;
        info.hasRealContact = false;
        info.bodyA = polyhedron;
        info.bodyB = capsule;
        info.contactCount = 0;
        info.normal = Vector3(0.0f, 1.0f, 0.0f);
        for (int i = 0; i < PhysicsConfig::MAX_CONTACT_POINTS; ++i)
        {
            info.contacts[i].pos = Vector3(0.0f, 0.0f, 0.0f);
            info.contacts[i].localPointA = Vector3(0.0f, 0.0f, 0.0f);
            info.contacts[i].localPointB = Vector3(0.0f, 0.0f, 0.0f);
            info.contacts[i].penetration = 0.0f;
            info.contacts[i].featureId = 0;
            info.contacts[i].lifetime = 0;
        }

        Vector3 capsuleP0, capsuleP1;
        capsuleEndpoints(capsule, capsuleP0, capsuleP1);
        const Quaternion inversePolyRotation = polyhedron->orientation.conjugate();
        capsuleP0 = inversePolyRotation.rotate(capsuleP0 - polyhedron->position);
        capsuleP1 = inversePolyRotation.rotate(capsuleP1 - polyhedron->position);

        float closestDistSq = FLT_MAX;
        Vector3 closestCapsulePoint(0.0f, 0.0f, 0.0f);
        Vector3 closestPolyPoint(0.0f, 0.0f, 0.0f);
        Vector3 closestFaceNormal(0.0f, 1.0f, 0.0f);
        int closestFace = 0;

        auto consider = [&](const Vector3 &capsulePoint, const Vector3 &polyPoint,
                            const Vector3 &faceNormal, int faceIdx) noexcept
        {
            const float distSq = (capsulePoint - polyPoint).lengthSquared();
            if (distSq < closestDistSq)
            {
                closestDistSq = distSq;
                closestCapsulePoint = capsulePoint;
                closestPolyPoint = polyPoint;
                closestFaceNormal = faceNormal;
                closestFace = faceIdx;
            }
        };

        const int faceCount = (polyhedron->shape == BODY_SHAPE_BOX) ? 6 : polyhedron->convexFaceCount;
        for (int faceIdx = 0; faceIdx < faceCount; ++faceIdx)
        {
            const ClipFace face = (polyhedron->shape == BODY_SHAPE_BOX)
                                      ? makeClipFaceFromBox(polyhedron, faceIdx)
                                      : makeClipFaceFromConvex(polyhedron, faceIdx);
            const Vector3 localNormal = inversePolyRotation.rotate(face.normal);
            const Vector3 a = inversePolyRotation.rotate(face.verts[0] - polyhedron->position);
            for (int i = 1; i + 1 < face.vertCount; ++i)
            {
                const Vector3 b = inversePolyRotation.rotate(face.verts[i] - polyhedron->position);
                const Vector3 c = inversePolyRotation.rotate(face.verts[i + 1] - polyhedron->position);

                Vector3 intersection;
                if (segmentIntersectsTriangle(capsuleP0, capsuleP1, a, b, c, intersection))
                    consider(intersection, intersection, localNormal, faceIdx);

                consider(capsuleP0, closestPointOnTriangle(capsuleP0, a, b, c), localNormal, faceIdx);
                consider(capsuleP1, closestPointOnTriangle(capsuleP1, a, b, c), localNormal, faceIdx);

                const Vector3 tri[3] = {a, b, c};
                for (int edge = 0; edge < 3; ++edge)
                {
                    Vector3 segmentPoint, edgePoint;
                    closestPointsOnSegments(capsuleP0, capsuleP1,
                                            tri[edge], tri[(edge + 1) % 3],
                                            segmentPoint, edgePoint);
                    consider(segmentPoint, edgePoint, localNormal, faceIdx);
                }
            }
        }

        if (closestDistSq == FLT_MAX)
            return info;
        const float distance = sqrtf(closestDistSq);
        if (distance > capsule->radius + PhysicsConfig::CONTACT_SKIN)
            return info;

        Vector3 localNormal = closestCapsulePoint - closestPolyPoint;
        const float normalLenSq = localNormal.lengthSquared();
        if (normalLenSq > 1e-12f)
            localNormal *= FastMath::fastInvSqrt(normalLenSq);
        else
            localNormal = closestFaceNormal;

        info.hasCollision = true;
        info.contactCount = 1;
        info.normal = polyhedron->orientation.rotate(localNormal);
        info.contacts[0].pos = polyhedron->position + polyhedron->orientation.rotate(closestPolyPoint);
        info.contacts[0].penetration = capsule->radius - distance > 0.0f
                                           ? capsule->radius - distance
                                           : 0.0f;
        info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_FACE,
                                                   static_cast<uint32_t>(closestFace));
        return info;
    }

    PIP3D_FORCE_INLINE uint32_t
    hashContactPos(const Vector3 &p, const Vector3 &center) noexcept
    {
        const Vector3 r = p - center;
        const int32_t qx = static_cast<int32_t>(r.x * 200.0f);
        const int32_t qy = static_cast<int32_t>(r.y * 200.0f);
        const int32_t qz = static_cast<int32_t>(r.z * 200.0f);
        uint32_t h = static_cast<uint32_t>(qx) * 73856093u;
        h ^= static_cast<uint32_t>(qy) * 19349663u;
        h ^= static_cast<uint32_t>(qz) * 83492791u;
        return h & 0x00FFFFFFu;
    }

    inline int clipIncidentToReference(const ClipFace &refFace,
                                       const ClipFace &incFace,
                                       float maxDepth,
                                       float contactSkin,
                                       Vector3 *outPoints,
                                       float *outDepths,
                                       int maxOut) noexcept
    {
        Vector3 poly[24];
        int polyCount = incFace.vertCount;
        if (polyCount > 24)
            polyCount = 24;
        for (int i = 0; i < polyCount; ++i)
            poly[i] = incFace.verts[i];

        for (int ei = 0; ei < refFace.vertCount && polyCount > 0; ++ei)
        {
            const int ej = (ei + 1) % refFace.vertCount;
            const Vector3 rv0 = refFace.verts[ei];
            const Vector3 rv1 = refFace.verts[ej];
            const Vector3 edgeDir = rv1 - rv0;

            Vector3 sideNormal = edgeDir.cross(refFace.normal);
            const float snLenSq = sideNormal.lengthSquared();
            if (snLenSq < 1e-12f)
                continue;
            sideNormal = sideNormal * FastMath::fastInvSqrt(snLenSq);
            const float sideD = sideNormal.dot(rv0);

            Vector3 newPoly[24];
            int newCount = 0;
            for (int i = 0; i < polyCount; ++i)
            {
                const int j = (i + 1) % polyCount;
                const Vector3 &a = poly[i];
                const Vector3 &b = poly[j];
                const float da = a.dot(sideNormal) - sideD;
                const float db = b.dot(sideNormal) - sideD;

                if (da <= 0.0f)
                {
                    if (db <= 0.0f)
                    {
                        if (newCount < 24)
                            newPoly[newCount++] = b;
                    }
                    else
                    {
                        const float denom = da - db;
                        const float t = (fabsf(denom) > 1e-12f) ? (da / denom) : 0.0f;
                        if (newCount < 24)
                            newPoly[newCount++] = a + (b - a) * t;
                    }
                }
                else
                {
                    if (db <= 0.0f)
                    {
                        const float denom = da - db;
                        const float t = (fabsf(denom) > 1e-12f) ? (da / denom) : 0.0f;
                        if (newCount < 24)
                            newPoly[newCount++] = a + (b - a) * t;
                        if (newCount < 24)
                            newPoly[newCount++] = b;
                    }
                }
            }

            for (int i = 0; i < newCount; ++i)
                poly[i] = newPoly[i];
            polyCount = newCount;
        }

        int outCount = 0;
        const float kMaxDepth = maxDepth + 0.1f;
        for (int i = 0; i < polyCount && outCount < maxOut; ++i)
        {
            const float dist = poly[i].dot(refFace.normal) - refFace.offset;
            const float depth = -dist;
            if (depth >= -contactSkin && depth <= kMaxDepth)
            {
                outPoints[outCount] = poly[i];
                outDepths[outCount] = depth > 0.0f ? depth : 0.0f;
                ++outCount;
            }
        }
        return outCount;
    }

    inline void sampleVertexContacts(const RigidBody *a, const RigidBody *b,
                                     const Vector3 &n, float maxDepth,
                                     Vector3 *outPoints, float *outDepths,
                                     int maxOut, int &outCount) noexcept
    {
        const float kDedupDistSq = 0.0025f * 0.0025f;
        const float skin = PhysicsConfig::CONTACT_SKIN;
        const float maxAllowedDepth = maxDepth + 0.1f;

        auto tryVertex = [&](const Vector3 &worldVert, const RigidBody *otherBody) -> void
        {
            if (outCount >= maxOut)
                return;

            const Vector3 localV = otherBody->orientation.conjugate().rotate(worldVert - otherBody->position);

            float minPen = FLT_MAX;
            const bool otherIsConvex = (otherBody->shape == BODY_SHAPE_CONVEX || otherBody->shape == BODY_SHAPE_CYLINDER);
            const bool otherIsBox = (otherBody->shape == BODY_SHAPE_BOX);

            if (otherIsConvex)
            {
                for (int i = 0; i < otherBody->convexFaceCount; ++i)
                {
                    const ConvexFace &f = otherBody->convexFaces[i];
                    const float sd = f.normal.dot(localV) - f.offset;
                    if (sd > skin)
                        return;
                    if (-sd < minPen)
                        minPen = -sd;
                }
            }
            else if (otherIsBox)
            {
                const Vector3 half = otherBody->size * 0.5f;
                if (localV.x > half.x + skin || localV.x < -half.x - skin)
                    return;
                if (localV.y > half.y + skin || localV.y < -half.y - skin)
                    return;
                if (localV.z > half.z + skin || localV.z < -half.z - skin)
                    return;
                const float dx = half.x - fabsf(localV.x);
                const float dy = half.y - fabsf(localV.y);
                const float dz = half.z - fabsf(localV.z);
                minPen = dx < dy ? (dx < dz ? dx : dz) : (dy < dz ? dy : dz);
            }
            else
                return;

            if (minPen <= 0.0f || minPen == FLT_MAX)
                return;
            if (minPen > maxAllowedDepth)
                minPen = maxAllowedDepth;

            for (int j = 0; j < outCount; ++j)
            {
                const Vector3 d = outPoints[j] - worldVert;
                if (d.lengthSquared() < kDedupDistSq)
                {
                    if (minPen > outDepths[j])
                    {
                        outPoints[j] = worldVert;
                        outDepths[j] = minPen;
                    }
                    return;
                }
            }

            outPoints[outCount] = worldVert;
            outDepths[outCount] = minPen;
            ++outCount;
        };

        const bool aHasVerts = (a->shape == BODY_SHAPE_CONVEX || a->shape == BODY_SHAPE_CYLINDER);
        const bool aIsBox = (a->shape == BODY_SHAPE_BOX);
        const bool bHasVerts = (b->shape == BODY_SHAPE_CONVEX || b->shape == BODY_SHAPE_CYLINDER);
        const bool bIsBox = (b->shape == BODY_SHAPE_BOX);

        if (aHasVerts)
        {
            for (int i = 0; i < a->convexCount && outCount < maxOut; ++i)
            {
                Vector3 wv = a->position + a->orientation.rotate(a->convexVerts[i]);
                tryVertex(wv, b);
            }
        }
        else if (aIsBox)
        {
            const Vector3 half = a->size * 0.5f;
            for (int i = 0; i < 8 && outCount < maxOut; ++i)
            {
                const float sx = (i & 1) ? 1.0f : -1.0f;
                const float sy = (i & 2) ? 1.0f : -1.0f;
                const float sz = (i & 4) ? 1.0f : -1.0f;
                Vector3 wv = a->position + a->orientation.rotate(Vector3(sx * half.x, sy * half.y, sz * half.z));
                tryVertex(wv, b);
            }
        }

        if (bHasVerts)
        {
            for (int i = 0; i < b->convexCount && outCount < maxOut; ++i)
            {
                Vector3 wv = b->position + b->orientation.rotate(b->convexVerts[i]);
                tryVertex(wv, a);
            }
        }
        else if (bIsBox)
        {
            const Vector3 half = b->size * 0.5f;
            for (int i = 0; i < 8 && outCount < maxOut; ++i)
            {
                const float sx = (i & 1) ? 1.0f : -1.0f;
                const float sy = (i & 2) ? 1.0f : -1.0f;
                const float sz = (i & 4) ? 1.0f : -1.0f;
                Vector3 wv = b->position + b->orientation.rotate(Vector3(sx * half.x, sy * half.y, sz * half.z));
                tryVertex(wv, a);
            }
        }
    }

    inline ContactManifold detectConvexConvex(RigidBody *a, RigidBody *b)
    {
        ContactManifold info;
        info.hasCollision = false;
        info.hasRealContact = false;
        info.bodyA = a;
        info.bodyB = b;
        info.contactCount = 0;
        info.normal = Vector3(0, 1, 0);
        for (int i = 0; i < PhysicsConfig::MAX_CONTACT_POINTS; ++i)
        {
            info.contacts[i].pos = Vector3(0, 0, 0);
            info.contacts[i].localPointA = Vector3(0, 0, 0);
            info.contacts[i].localPointB = Vector3(0, 0, 0);
            info.contacts[i].penetration = 0.0f;
            info.contacts[i].featureId = 0;
            info.contacts[i].lifetime = 0;
        }
        if (!a || !b)
            return info;

        if (isPolyhedral(a) && b->shape == BODY_SHAPE_SPHERE)
            return detectPolyhedronSphere(a, b);

        if (isPolyhedral(a) && b->shape == BODY_SHAPE_CAPSULE)
            return detectPolyhedronCapsule(a, b);

        if (a->shape == BODY_SHAPE_SPHERE && isPolyhedral(b))
        {
            ContactManifold swapped = detectPolyhedronSphere(b, a);
            if (!swapped.hasCollision)
                return info;
            swapped.bodyA = a;
            swapped.bodyB = b;
            swapped.normal = -swapped.normal;
            return swapped;
        }

        if (a->shape == BODY_SHAPE_CAPSULE && isPolyhedral(b))
        {
            ContactManifold swapped = detectPolyhedronCapsule(b, a);
            if (!swapped.hasCollision)
                return info;
            swapped.bodyA = a;
            swapped.bodyB = b;
            swapped.normal = -swapped.normal;
            return swapped;
        }

        const bool aHasHull = (a->shape == BODY_SHAPE_CONVEX || a->shape == BODY_SHAPE_CYLINDER) &&
                              a->convexHullComplete && a->convexFaceCount >= 4;
        const bool bHasHull = (b->shape == BODY_SHAPE_CONVEX || b->shape == BODY_SHAPE_CYLINDER) &&
                              b->convexHullComplete && b->convexFaceCount >= 4;
        const bool aIsBox = (a->shape == BODY_SHAPE_BOX);
        const bool bIsBox = (b->shape == BODY_SHAPE_BOX);

        Vector3 penetrationNormal(0.0f, 1.0f, 0.0f);
        float penetrationDepth = 0.0f;
        Vector3 fallbackPointA(0.0f, 0.0f, 0.0f);
        Vector3 fallbackPointB(0.0f, 0.0f, 0.0f);

        if (isPolyhedral(a) && isPolyhedral(b))
        {
            if (!findPolyhedralPenetrationAxis(a, b, penetrationNormal, penetrationDepth))
                return info;
            fallbackPointA = a->support(penetrationNormal);
            fallbackPointB = b->support(-penetrationNormal);
        }
        else
        {
            GJKVertex simplex[4];
            int simplexSize = 0;
            if (!gjkIntersect(a, b, simplex, simplexSize))
                return info;

            EPAResult epa;
            if (!epaPenetration(a, b, simplex, simplexSize, epa))
                return info;
            penetrationNormal = epa.normal;
            penetrationDepth = epa.depth;
            fallbackPointA = epa.contactPointA;
            fallbackPointB = epa.contactPointB;
        }

        info.hasCollision = true;
        info.bodyA = a;
        info.bodyB = b;
        info.normal = penetrationNormal;

        if (!(aHasHull || aIsBox) || !(bHasHull || bIsBox))
        {

            info.contactCount = 1;
            info.contacts[0].pos = (fallbackPointA + fallbackPointB) * 0.5f;
            info.contacts[0].penetration = penetrationDepth;
            info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, 0u);
            info.contacts[0].lifetime = 0;
            return info;
        }

        const Vector3 n = penetrationNormal;

        const int bestFaceA = aHasHull ? findBestConvexFaceAlignment(a, n) : findBestBoxFaceAlignment(a, n);
        const int bestFaceB = bHasHull ? findBestConvexFaceAlignment(b, -n) : findBestBoxFaceAlignment(b, -n);

        if (bestFaceA < 0 || bestFaceB < 0)
        {
            info.contactCount = 1;
            info.contacts[0].pos = (fallbackPointA + fallbackPointB) * 0.5f;
            info.contacts[0].penetration = penetrationDepth;
            info.contacts[0].featureId = makeFeatureId(CONTACT_FEATURE_VERTEX, 0u);
            info.contacts[0].lifetime = 0;
            return info;
        }

        const Vector3 wnA = aHasHull
                                ? a->orientation.rotate(a->convexFaces[bestFaceA].normal)
                                : boxFaceNormalWorld(a, bestFaceA);
        const Vector3 wnB = bHasHull
                                ? b->orientation.rotate(b->convexFaces[bestFaceB].normal)
                                : boxFaceNormalWorld(b, bestFaceB);
        const float dotA = wnA.dot(n);
        const float dotB = wnB.dot(-n);

        const bool aIsReference = (dotA >= dotB);
        RigidBody *refBody = aIsReference ? a : b;
        RigidBody *incBody = aIsReference ? b : a;
        const int refFaceIdx = aIsReference ? bestFaceA : bestFaceB;
        const int incFaceIdx = aIsReference ? bestFaceB : bestFaceA;

        const bool refHasHull = (refBody->shape == BODY_SHAPE_CONVEX || refBody->shape == BODY_SHAPE_CYLINDER) &&
                                refBody->convexHullComplete && refBody->convexFaceCount >= 4;
        const bool incHasHull = (incBody->shape == BODY_SHAPE_CONVEX || incBody->shape == BODY_SHAPE_CYLINDER) &&
                                incBody->convexHullComplete && incBody->convexFaceCount >= 4;

        const ClipFace refFace = refHasHull ? makeClipFaceFromConvex(refBody, refFaceIdx) : makeClipFaceFromBox(refBody, refFaceIdx);
        const ClipFace incFace = incHasHull ? makeClipFaceFromConvex(incBody, incFaceIdx) : makeClipFaceFromBox(incBody, incFaceIdx);

        Vector3 clippedPoints[16];
        float clippedDepths[16];
        const int clipCount = clipIncidentToReference(
            refFace, incFace, penetrationDepth, PhysicsConfig::CONTACT_SKIN,
            clippedPoints, clippedDepths, 16);

        Vector3 finalPoints[8];
        float finalDepths[8];
        int finalCount = 0;
        const float kDedupDistSq = 0.0025f * 0.0025f;

        auto addPoint = [&](const Vector3 &p, float d)
        {
            if (finalCount >= 8)
                return;
            for (int j = 0; j < finalCount; ++j)
            {
                const Vector3 diff = finalPoints[j] - p;
                if (diff.lengthSquared() < kDedupDistSq)
                {
                    if (d > finalDepths[j])
                    {
                        finalPoints[j] = p;
                        finalDepths[j] = d;
                    }
                    return;
                }
            }
            finalPoints[finalCount] = p;
            finalDepths[finalCount] = d;
            ++finalCount;
        };

        for (int i = 0; i < clipCount; ++i)
            addPoint(clippedPoints[i], clippedDepths[i]);

        for (int i = 1; i < finalCount; ++i)
        {
            const Vector3 p = finalPoints[i];
            const float d = finalDepths[i];
            int j = i - 1;
            while (j >= 0 && finalDepths[j] < d)
            {
                finalPoints[j + 1] = finalPoints[j];
                finalDepths[j + 1] = finalDepths[j];
                --j;
            }
            finalPoints[j + 1] = p;
            finalDepths[j + 1] = d;
        }
        if (finalCount > 4)
            finalCount = 4;

        if (finalCount == 0)
        {
            finalPoints[0] = (fallbackPointA + fallbackPointB) * 0.5f;
            finalDepths[0] = penetrationDepth;
            finalCount = 1;
        }

        info.contactCount = finalCount;
        for (int i = 0; i < finalCount; ++i)
        {
            Contact &c = info.contacts[i];
            c.pos = finalPoints[i];
            c.penetration = finalDepths[i];
            const uint32_t featureKey =
                (static_cast<uint32_t>(refFaceIdx & 0xFF) << 16) |
                (static_cast<uint32_t>(incFaceIdx & 0xFF) << 8) |
                static_cast<uint32_t>(i);
            c.featureId = makeFeatureId(CONTACT_FEATURE_FACE, featureKey);
            c.lifetime = 0;
            c.accumulatedImpulse = 0.0f;
            c.tangentImpulse1 = 0.0f;
            c.tangentImpulse2 = 0.0f;
        }

        return info;
    }

    inline ContactManifold detectConvexSphere(RigidBody *convex, RigidBody *sphere)
    {
        return detectConvexConvex(convex, sphere);
    }

    inline ContactManifold detectConvexBox(RigidBody *convex, RigidBody *box)
    {
        return detectConvexConvex(convex, box);
    }

}

#pragma once

#include <float.h>
#include <math.h>

#include "Math/Collision.hpp"
#include "../Dynamics/Body.hpp"
#include "../Dynamics/Contacts.hpp"
#include "../Types.hpp"
#include "GJK.hpp"
#include "Simplex.hpp"
#include "EPA.hpp"

namespace pip3D
{
    __attribute__((always_inline)) inline void
    capsuleEndpoints(const RigidBody *cap, Vector3 &outP0, Vector3 &outP1)
    {
        Vector3 axisY = cap->orientation.rotate(Vector3(0.0f, cap->capsuleHalfHeight, 0.0f));
        outP0 = cap->position - axisY;
        outP1 = cap->position + axisY;
    }

    __attribute__((always_inline)) inline void
    closestPtSegmentSegment(const Vector3 &p1, const Vector3 &q1,
                            const Vector3 &p2, const Vector3 &q2,
                            float &s, float &t,
                            Vector3 &c1, Vector3 &c2)
    {
        const float kSegEps = 1e-8f;
        Vector3 d1 = q1 - p1;
        Vector3 d2 = q2 - p2;
        Vector3 r = p1 - p2;
        float a = d1.dot(d1);
        float e = d2.dot(d2);
        float f = d2.dot(r);

        if (a <= kSegEps && e <= kSegEps)
        {
            s = 0.0f;
            t = 0.0f;
            c1 = p1;
            c2 = p2;
            return;
        }
        if (a <= kSegEps)
        {
            s = 0.0f;
            t = f / e;
            if (t < 0.0f)
                t = 0.0f;
            else if (t > 1.0f)
                t = 1.0f;
        }
        else
        {
            float c = d1.dot(r);
            if (e <= kSegEps)
            {
                t = 0.0f;
                s = -c / a;
                if (s < 0.0f)
                    s = 0.0f;
                else if (s > 1.0f)
                    s = 1.0f;
            }
            else
            {
                float b = d1.dot(d2);
                float denom = a * e - b * b;
                if (denom != 0.0f)
                {
                    s = (b * f - c * e) / denom;
                    if (s < 0.0f)
                        s = 0.0f;
                    else if (s > 1.0f)
                        s = 1.0f;
                }
                else
                {
                    s = 0.0f;
                }
                t = (b * s + f) / e;
                if (t < 0.0f)
                {
                    t = 0.0f;
                    s = (-c) / a;
                    if (s < 0.0f)
                        s = 0.0f;
                    else if (s > 1.0f)
                        s = 1.0f;
                }
                else if (t > 1.0f)
                {
                    t = 1.0f;
                    s = (b - c) / a;
                    if (s < 0.0f)
                        s = 0.0f;
                    else if (s > 1.0f)
                        s = 1.0f;
                }
            }
        }
        c1 = p1 + d1 * s;
        c2 = p2 + d2 * t;
    }

    __attribute__((always_inline)) inline Vector3
    closestPtOnSegment(const Vector3 &P, const Vector3 &A, const Vector3 &B, float &t)
    {
        Vector3 ab = B - A;
        float abLenSq = ab.lengthSquared();
        if (abLenSq <= 1e-10f)
        {
            t = 0.0f;
            return A;
        }
        t = (P - A).dot(ab) / abLenSq;
        if (t < 0.0f)
            t = 0.0f;
        else if (t > 1.0f)
            t = 1.0f;
        return A + ab * t;
    }

    __attribute__((always_inline)) inline Vector3
    closestPtOnAABB(const Vector3 &p, const Vector3 &mn, const Vector3 &mx) noexcept
    {
        return Vector3(
            (p.x < mn.x) ? mn.x : (p.x > mx.x ? mx.x : p.x),
            (p.y < mn.y) ? mn.y : (p.y > mx.y ? mx.y : p.y),
            (p.z < mn.z) ? mn.z : (p.z > mx.z ? mx.z : p.z));
    }

    __attribute__((always_inline)) inline Vector3
    closestPtOnBox(const Vector3 &p, const RigidBody *box, Vector3 &outLocal) noexcept
    {
        Vector3 boxCenter = box->position;
        Vector3 halfExtents = box->size * 0.5f;
        Quaternion invRot = box->orientation.conjugate();
        Vector3 local = invRot.rotate(p - boxCenter);
        Vector3 clampedLocal(
            (local.x < -halfExtents.x) ? -halfExtents.x : (local.x > halfExtents.x ? halfExtents.x : local.x),
            (local.y < -halfExtents.y) ? -halfExtents.y : (local.y > halfExtents.y ? halfExtents.y : local.y),
            (local.z < -halfExtents.z) ? -halfExtents.z : (local.z > halfExtents.z ? halfExtents.z : local.z));
        outLocal = clampedLocal;
        return box->orientation.rotate(clampedLocal) + boxCenter;
    }

    __attribute__((always_inline)) inline float
    closestSegAABB(const Vector3 &A, const Vector3 &B,
                   const Vector3 &mn, const Vector3 &mx,
                   Vector3 &segPt, Vector3 &boxPt)
    {
        Vector3 dir = B - A;
        float candidates[8];
        int n = 0;
        candidates[n++] = 0.0f;
        candidates[n++] = 1.0f;

        for (int k = 0; k < 3; ++k)
        {
            float d = (k == 0) ? dir.x : (k == 1) ? dir.y
                                                  : dir.z;
            if (fabsf(d) > 1e-8f)
            {
                float a = (k == 0) ? A.x : (k == 1) ? A.y
                                                    : A.z;
                float lo = (k == 0) ? mn.x : (k == 1) ? mn.y
                                                      : mn.z;
                float hi = (k == 0) ? mx.x : (k == 1) ? mx.y
                                                      : mx.z;
                candidates[n++] = (lo - a) / d;
                candidates[n++] = (hi - a) / d;
            }
        }

        float bestDistSq = FLT_MAX;
        Vector3 bestSeg = A, bestBox = A;
        for (int i = 0; i < n; ++i)
        {
            float tc = candidates[i];
            if (tc < 0.0f)
                tc = 0.0f;
            else if (tc > 1.0f)
                tc = 1.0f;
            Vector3 p = A + dir * tc;
            Vector3 c = closestPtOnAABB(p, mn, mx);
            float dx = p.x - c.x, dy = p.y - c.y, dz = p.z - c.z;
            float dSq = dx * dx + dy * dy + dz * dz;
            if (dSq < bestDistSq)
            {
                bestDistSq = dSq;
                bestSeg = p;
                bestBox = c;
            }
        }
        segPt = bestSeg;
        boxPt = bestBox;
        return bestDistSq;
    }

    __attribute__((always_inline)) inline bool
    spherePointContactVsBox(const Vector3 &sphereCenter, float sphereR,
                            RigidBody *box,
                            Vector3 &outNormal, float &outPen,
                            Vector3 &outContactWorld) noexcept
    {
        Vector3 localClosest;
        Vector3 closestWorld = closestPtOnBox(sphereCenter, box, localClosest);
        Vector3 diff = closestWorld - sphereCenter;
        float distSq = diff.lengthSquared();
        if (distSq > sphereR * sphereR)
            return false;

        float dist = distSq > 1e-8f ? sqrtf(distSq) : 0.0f;
        if (dist > 1e-4f)
        {
            outNormal = diff * FastMath::fastInvSqrt(distSq);
            outPen = sphereR - dist;
            outContactWorld = closestWorld;
            return true;
        }

        Vector3 boxCenter = box->position;
        Vector3 halfExtents = box->size * 0.5f;
        Quaternion invRot = box->orientation.conjugate();
        Vector3 local = invRot.rotate(sphereCenter - boxCenter);
        float dx = halfExtents.x - fabsf(local.x);
        float dy = halfExtents.y - fabsf(local.y);
        float dz = halfExtents.z - fabsf(local.z);
        Vector3 localN(0, -1, 0);
        if (dx < dy && dx < dz)
            localN = Vector3((local.x > 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);
        else if (dy < dz)
            localN = Vector3(0.0f, (local.y > 0.0f) ? 1.0f : -1.0f, 0.0f);
        else
            localN = Vector3(0.0f, 0.0f, (local.z > 0.0f) ? 1.0f : -1.0f);
        outNormal = box->orientation.rotate(localN);
        outPen = sphereR + fminf(fminf(dx, dy), dz);
        outContactWorld = closestWorld;
        return true;
    }
}

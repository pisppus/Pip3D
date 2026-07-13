#pragma once

#include "Math/Algebra.hpp"
#include "Math/Collision.hpp"
#include "Geometry/Mesh.hpp"
#include "Geometry/Instance.hpp"
#include "../Dynamics/Body.hpp"
#include "../Types.hpp"
#include "ConvexHull.hpp"

#include <cfloat>

namespace pip3D
{

    inline AABB getMeshLocalAABB(const Mesh &mesh)
    {
        const uint16_t n = mesh.numVertices();
        if (n == 0)
            return AABB(Vector3(0, 0, 0), Vector3(0, 0, 0));

        const Vertex *v = mesh.vertexData();
        Vector3 p0 = mesh.decodePosition(v[0]);
        Vector3 mn(p0), mx(p0);
        for (uint16_t i = 1; i < n; ++i)
        {
            const Vector3 p = mesh.decodePosition(v[i]);
            if (p.x < mn.x)
                mn.x = p.x;
            else if (p.x > mx.x)
                mx.x = p.x;
            if (p.y < mn.y)
                mn.y = p.y;
            else if (p.y > mx.y)
                mx.y = p.y;
            if (p.z < mn.z)
                mn.z = p.z;
            else if (p.z > mx.z)
                mx.z = p.z;
        }
        return AABB(mn, mx);
    }

    inline AABB getMeshWorldAABB(const MeshInstance &inst)
    {
        Mesh *mesh = inst.getMesh();
        if (!mesh)
        {
            const Vector3 &p = inst.pos();
            return AABB(p, p);
        }

        const AABB local = getMeshLocalAABB(*mesh);
        const Vector3 &scl = inst.getScale();
        const Quaternion &rot = inst.getRotation();
        const Vector3 &pos = inst.pos();

        const Vector3 corners[8] = {
            Vector3(local.min.x * scl.x, local.min.y * scl.y, local.min.z * scl.z),
            Vector3(local.max.x * scl.x, local.min.y * scl.y, local.min.z * scl.z),
            Vector3(local.min.x * scl.x, local.max.y * scl.y, local.min.z * scl.z),
            Vector3(local.max.x * scl.x, local.max.y * scl.y, local.min.z * scl.z),
            Vector3(local.min.x * scl.x, local.min.y * scl.y, local.max.z * scl.z),
            Vector3(local.max.x * scl.x, local.min.y * scl.y, local.max.z * scl.z),
            Vector3(local.min.x * scl.x, local.max.y * scl.y, local.max.z * scl.z),
            Vector3(local.max.x * scl.x, local.max.y * scl.y, local.max.z * scl.z)};

        Vector3 wMin = rot.rotate(corners[0]) + pos;
        Vector3 wMax = wMin;
        for (int i = 1; i < 8; ++i)
        {
            const Vector3 w = rot.rotate(corners[i]) + pos;
            if (w.x < wMin.x)
                wMin.x = w.x;
            else if (w.x > wMax.x)
                wMax.x = w.x;
            if (w.y < wMin.y)
                wMin.y = w.y;
            else if (w.y > wMax.y)
                wMax.y = w.y;
            if (w.z < wMin.z)
                wMin.z = w.z;
            else if (w.z > wMax.z)
                wMax.z = w.z;
        }
        return AABB(wMin, wMax);
    }

    namespace detail
    {
        enum class AABBClass
        {
            PLANAR,
            ELONGATED,
            SOLID
        };

        struct AABBInfo
        {
            Vector3 min;
            Vector3 max;
            Vector3 extents;
            Vector3 center;
            float e[3];
            int axis[3];
            int smallAxis;
            int midAxis;
            int largeAxis;
            float small, mid, large;
            AABBClass kind;

            static constexpr float kPlanarRatio = 0.10f;
            static constexpr float kElongatedRatio = 1.5f;
            static constexpr float kMinPlanarThickness = 0.05f;
            static constexpr float kMinExtent = 0.005f;
        };

        inline AABBInfo classifyAABB(const Vector3 &mn, const Vector3 &mx) noexcept
        {
            AABBInfo c;
            c.min = mn;
            c.max = mx;
            c.extents = mx - mn;
            c.center = (mx + mn) * 0.5f;

            c.e[0] = c.extents.x;
            c.e[1] = c.extents.y;
            c.e[2] = c.extents.z;
            c.axis[0] = 0;
            c.axis[1] = 1;
            c.axis[2] = 2;

            if (c.e[c.axis[0]] > c.e[c.axis[1]])
            {
                int t = c.axis[0];
                c.axis[0] = c.axis[1];
                c.axis[1] = t;
            }
            if (c.e[c.axis[1]] > c.e[c.axis[2]])
            {
                int t = c.axis[1];
                c.axis[1] = c.axis[2];
                c.axis[2] = t;
            }
            if (c.e[c.axis[0]] > c.e[c.axis[1]])
            {
                int t = c.axis[0];
                c.axis[0] = c.axis[1];
                c.axis[1] = t;
            }

            c.smallAxis = c.axis[0];
            c.midAxis = c.axis[1];
            c.largeAxis = c.axis[2];
            c.small = c.e[c.axis[0]];
            c.mid = c.e[c.axis[1]];
            c.large = c.e[c.axis[2]];

            if (c.large > 1e-4f && c.small < AABBInfo::kPlanarRatio * c.large)
                c.kind = AABBClass::PLANAR;
            else if (c.large > AABBInfo::kElongatedRatio * c.mid && c.mid > 1e-4f)
                c.kind = AABBClass::ELONGATED;
            else
                c.kind = AABBClass::SOLID;
            return c;
        }

        inline Quaternion alignYToAxis(int axis) noexcept
        {
            if (axis == 1)
                return Quaternion();
            if (axis == 0)
                return Quaternion::fromAxisAngle(Vector3(0, 0, 1), -kHalfPi);
            return Quaternion::fromAxisAngle(Vector3(1, 0, 0), kHalfPi);
        }
    }

    inline void fitBodyToMesh(RigidBody *body, const MeshInstance &inst,
                              float inflate = 0.0f)
    {
        if (!body)
            return;
        Mesh *mesh = inst.getMesh();
        if (!mesh)
            return;

        const AABB local = getMeshLocalAABB(*mesh);
        const Vector3 scl = inst.getScale();
        Vector3 mn(local.min.x * scl.x, local.min.y * scl.y, local.min.z * scl.z);
        Vector3 mx(local.max.x * scl.x, local.max.y * scl.y, local.max.z * scl.z);
        if (inflate > 0.0f)
        {
            mn -= Vector3(inflate, inflate, inflate);
            mx += Vector3(inflate, inflate, inflate);
        }
        Vector3 ext = mx - mn;
        if (ext.x < detail::AABBInfo::kMinExtent)
            ext.x = detail::AABBInfo::kMinExtent;
        if (ext.y < detail::AABBInfo::kMinExtent)
            ext.y = detail::AABBInfo::kMinExtent;
        if (ext.z < detail::AABBInfo::kMinExtent)
            ext.z = detail::AABBInfo::kMinExtent;
        const Vector3 aabbCenter = (mx + mn) * 0.5f;

        const detail::AABBInfo info = detail::classifyAABB(mn, mx);

        const Quaternion instRot = inst.getRotation();

        if (info.kind == detail::AABBClass::PLANAR)
        {
            Vector3 boxExt = ext;
            float &smallDim = (info.smallAxis == 0)   ? boxExt.x
                              : (info.smallAxis == 1) ? boxExt.y
                                                      : boxExt.z;
            if (smallDim < detail::AABBInfo::kMinPlanarThickness)
                smallDim = detail::AABBInfo::kMinPlanarThickness;

            body->setBox(boxExt);
            body->orientation = instRot;
            body->updateWorldInvInertia();
            body->updateBoundsFromTransform();
            return;
        }

        if (info.kind == detail::AABBClass::ELONGATED)
        {
            const int la = info.largeAxis;
            float offA = (la == 0) ? ext.y : ext.x;
            float offB = (la == 0) ? ext.z : (la == 1 ? ext.z : ext.y);
            float capRadius = 0.5f * (offA < offB ? offA : offB);
            float capHalfHeight = info.large * 0.5f - capRadius;
            if (capHalfHeight < 0.0f)
                capHalfHeight = 0.0f;

            const Quaternion localAlign = detail::alignYToAxis(la);
            body->setCapsule(capRadius, capHalfHeight);
            body->orientation = instRot * localAlign;
            body->updateWorldInvInertia();
            body->updateBoundsFromTransform();
            return;
        }

        {
            Vector3 cv[RigidBody::kMaxConvexVerts];
            int cvCount = 0;
            extractConvexVertices(*mesh, scl, aabbCenter, cv, &cvCount,
                                  RigidBody::kMaxConvexVerts);

            bool nonCoplanar = (cvCount >= 4);
            if (nonCoplanar)
            {
                Vector3 hmn = cv[0], hmx = cv[0];
                for (int i = 1; i < cvCount; ++i)
                {
                    if (cv[i].x < hmn.x)
                        hmn.x = cv[i].x;
                    else if (cv[i].x > hmx.x)
                        hmx.x = cv[i].x;
                    if (cv[i].y < hmn.y)
                        hmn.y = cv[i].y;
                    else if (cv[i].y > hmx.y)
                        hmx.y = cv[i].y;
                    if (cv[i].z < hmn.z)
                        hmn.z = cv[i].z;
                    else if (cv[i].z > hmx.z)
                        hmx.z = cv[i].z;
                }
                const Vector3 hext = hmx - hmn;
                const float minHext = (hext.x < hext.y) ? hext.x : hext.y;
                const float minH = (minHext < hext.z) ? minHext : hext.z;
                if (minH < detail::AABBInfo::kMinExtent)
                    nonCoplanar = false;
            }

            bool isBoxShape = false;
            if (nonCoplanar && cvCount == 8)
            {
                const Vector3 half = ext * 0.5f;
                const Vector3 kBoxCorners[8] = {
                    Vector3(-half.x, -half.y, -half.z),
                    Vector3(half.x, -half.y, -half.z),
                    Vector3(-half.x, half.y, -half.z),
                    Vector3(half.x, half.y, -half.z),
                    Vector3(-half.x, -half.y, half.z),
                    Vector3(half.x, -half.y, half.z),
                    Vector3(-half.x, half.y, half.z),
                    Vector3(half.x, half.y, half.z)};
                const float kCornerEps = 1e-3f;
                const float kCornerEpsSq = kCornerEps * kCornerEps;
                int matched = 0;
                bool usedCv[8] = {false, false, false, false, false, false, false, false};
                for (int i = 0; i < 8; ++i)
                {
                    for (int j = 0; j < 8; ++j)
                    {
                        if (usedCv[j])
                            continue;
                        const Vector3 d = cv[j] - kBoxCorners[i];
                        if (d.lengthSquared() < kCornerEpsSq)
                        {
                            usedCv[j] = true;
                            ++matched;
                            break;
                        }
                    }
                }
                isBoxShape = (matched == 8);
            }

            if (nonCoplanar && !isBoxShape)
            {
                body->setConvex(cv, cvCount);
                body->orientation = instRot;
                body->updateWorldInvInertia();
                body->updateBoundsFromTransform();
                return;
            }
            if (nonCoplanar && isBoxShape)
            {
                body->setBox(ext);
                body->orientation = instRot;
                body->updateWorldInvInertia();
                body->updateBoundsFromTransform();
                return;
            }
        }

        body->setBox(ext);
        body->orientation = instRot;
        body->updateWorldInvInertia();
        body->updateBoundsFromTransform();
    }
}

#pragma once

#include "Math/Algebra.hpp"
#include "Math/Collision.hpp"
#include "Geometry/Mesh.hpp"
#include "Geometry/Instance.hpp"
#include "../RigidBody/Body.hpp"
#include "../Types.hpp"
#include "Hull.hpp"

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

}

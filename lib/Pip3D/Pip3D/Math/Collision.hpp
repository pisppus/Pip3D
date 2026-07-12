#pragma once

#include "Algebra.hpp"
#include "Core/Platform.hpp"

namespace pip3D
{
  struct AABB
  {
    Vector3 min;
    Vector3 max;

    constexpr AABB() noexcept : min(0, 0, 0), max(0, 0, 0) {}
    constexpr AABB(const Vector3 &mn, const Vector3 &mx) noexcept
        : min(mn), max(mx) {}

    PIP3D_FORCE_INLINE static constexpr AABB
    fromCenterSize(const Vector3 &center, const Vector3 &size) noexcept
    {
      const Vector3 half = size * 0.5f;
      return AABB(center - half, center + half);
    }

    [[nodiscard]] PIP3D_FORCE_INLINE constexpr bool
    contains(const Vector3 &p) const noexcept
    {
      return p.x >= min.x && p.x <= max.x &&
             p.y >= min.y && p.y <= max.y &&
             p.z >= min.z && p.z <= max.z;
    }

    [[nodiscard]] PIP3D_FORCE_INLINE constexpr bool
    intersects(const AABB &o) const noexcept
    {
      return min.x <= o.max.x && max.x >= o.min.x &&
             min.y <= o.max.y && max.y >= o.min.y &&
             min.z <= o.max.z && max.z >= o.min.z;
    }
  };
  static_assert(sizeof(AABB) == 24);
  static_assert(alignof(AABB) == 4);

  struct CollisionSphere
  {
    Vector3 center;
    float radius;

    constexpr CollisionSphere() noexcept : center(0, 0, 0), radius(0) {}
    constexpr CollisionSphere(const Vector3 &c, float r) noexcept
        : center(c), radius(r) {}
  };
  static_assert(sizeof(CollisionSphere) == 16);
  static_assert(alignof(CollisionSphere) == 4);

  struct Ray
  {

    Vector3 origin;
    Vector3 direction;
    Vector3 invDirection;

    constexpr Ray() noexcept
        : origin(0, 0, 0),
          direction(0, 0, 1),
          invDirection(__builtin_inff(), __builtin_inff(), 1.0f) {}

    PIP3D_FORCE_INLINE Ray(const Vector3 &o, const Vector3 &d) noexcept
        : origin(o), direction(d)
    {
      invDirection.x = FastMath::fastReciprocal(d.x);
      invDirection.y = FastMath::fastReciprocal(d.y);
      invDirection.z = FastMath::fastReciprocal(d.z);
    }

    PIP3D_FORCE_INLINE constexpr Vector3 at(float t) const noexcept
    {
      return origin + direction * t;
    }

    [[nodiscard]] PIP3D_FORCE_INLINE bool
    intersects(const AABB &box, float &tMinOut, float &tMaxOut) const noexcept
    {
      const float t0x = (box.min.x - origin.x) * invDirection.x;
      const float t1x = (box.max.x - origin.x) * invDirection.x;
      const float t0y = (box.min.y - origin.y) * invDirection.y;
      const float t1y = (box.max.y - origin.y) * invDirection.y;
      const float t0z = (box.min.z - origin.z) * invDirection.z;
      const float t1z = (box.max.z - origin.z) * invDirection.z;

      const float loX = t0x < t1x ? t0x : t1x;
      const float hiX = t0x < t1x ? t1x : t0x;
      const float loY = t0y < t1y ? t0y : t1y;
      const float hiY = t0y < t1y ? t1y : t0y;
      const float loZ = t0z < t1z ? t0z : t1z;
      const float hiZ = t0z < t1z ? t1z : t0z;

      float tMin = loX > loY ? loX : loY;
      tMin = tMin > loZ ? tMin : loZ;

      float tMax = hiX < hiY ? hiX : hiY;
      tMax = tMax < hiZ ? tMax : hiZ;

      tMinOut = tMin;
      tMaxOut = tMax;
      return tMax >= tMin;
    }

    [[nodiscard]] PIP3D_FORCE_INLINE bool
    intersects(const CollisionSphere &s, float &t) const noexcept
    {
      const Vector3 oc = origin - s.center;
      const float a = direction.lengthSquared();

      if (unlikely(a <= 1e-8f))
      {
        const float distSq = oc.lengthSquared();
        const float rSq = s.radius * s.radius;
        if (distSq <= rSq)
        {
          t = 0.0f;
          return true;
        }
        return false;
      }

      const float halfB = oc.dot(direction);
      const float c = oc.lengthSquared() - s.radius * s.radius;
      const float disc = halfB * halfB - a * c;

      if (unlikely(disc < 0.0f))
        return false;

      const float invSqrtD = FastMath::fastInvSqrt(disc);
      const float sqrtD = disc * invSqrtD;
      const float invA = FastMath::fastReciprocal(a);

      const float t0 = (-halfB - sqrtD) * invA;
      if (t0 >= 0.0f)
      {
        t = t0;
        return true;
      }

      const float t1 = (-halfB + sqrtD) * invA;
      if (t1 >= 0.0f)
      {
        t = t1;
        return true;
      }

      return false;
    }
  };
  static_assert(sizeof(Ray) == 36);
  static_assert(alignof(Ray) == 4);
}
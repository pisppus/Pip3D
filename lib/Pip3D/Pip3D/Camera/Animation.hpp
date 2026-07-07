#pragma once

#include "Math/Algebra.hpp"

namespace pip3D
{
  struct CameraAnimation
  {
    Vector3 startPos, startTgt, startUp;
    Vector3 deltaPos, deltaTgt, deltaUp;
    float startFov = 60.0f;
    float deltaFov = 0.0f;
    float time = 0.0f;
    float duration = 1.0f;
    float invDuration = 1.0f;
    uint8_t dirtyMask = 0;

    enum Type : uint8_t
    {
      LINEAR,
      SMOOTH,
      EASE
    } type = SMOOTH;

    bool active = false;

    CameraAnimation() = default;

    PIP3D_FORCE_INLINE void reset(const Vector3 &fromPos, const Vector3 &fromTgt,
                                  const Vector3 &fromUp, float fromFov,
                                  const Vector3 &toPos, const Vector3 &toTgt,
                                  const Vector3 &toUp, float toFov,
                                  float dur, Type t)
    {
      startPos = fromPos;
      startTgt = fromTgt;
      startUp = fromUp;
      startFov = fromFov;
      deltaPos = toPos - fromPos;
      deltaTgt = toTgt - fromTgt;
      deltaUp = toUp - fromUp;
      deltaFov = toFov - fromFov;
      duration = dur;
      invDuration = (dur > 0.0f) ? FastMath::fastReciprocal(dur) : 0.0f;
      time = 0.0f;
      type = t;
      active = true;

      dirtyMask = 0x04;
      if (deltaPos.lengthSquared() + deltaTgt.lengthSquared() + deltaUp.lengthSquared() > 0.0f)
      {
        dirtyMask |= 0x01;
        dirtyMask |= 0x10;
      }
      if (deltaFov != 0.0f)
        dirtyMask |= 0x02;
    }

    PIP3D_FORCE_INLINE float tick(float dt)
    {
      time += dt;
      float t = (duration > 0.0f) ? time * invDuration : 1.0f;
      if (t >= 1.0f)
      {
        t = 1.0f;
        active = false;
      }

      if (type == SMOOTH)
        return t * t * (3.0f - 2.0f * t);
      if (type == EASE)
        return (t < 0.5f) ? (2.0f * t * t)
                          : (1.0f - 2.0f * (1.0f - t) * (1.0f - t));
      return t;
    }

    PIP3D_FORCE_INLINE void stop() { active = false; }
    PIP3D_FORCE_INLINE bool isActive() const { return active; }
  };
}

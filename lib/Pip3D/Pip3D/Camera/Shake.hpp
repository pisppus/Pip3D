#pragma once

#include <cmath>
#include "Math/Algebra.hpp"

namespace pip3D
{
  struct CameraShake
  {
    float trauma = 0.0f;
    float decay = 1.3f;
    float timePhase = 0.0f;
    float frequency = 25.0f;

    float maxPitch = 12.0f;
    float maxYaw = 12.0f;
    float maxRoll = 8.0f;
    float maxOffsetX = 0.8f;
    float maxOffsetY = 0.8f;
    float maxOffsetZ = 0.5f;

    PIP3D_FORCE_INLINE void addTrauma(float amount)
    {
      trauma += amount;
      if (trauma > 1.0f)
        trauma = 1.0f;
      else if (trauma < 0.0f)
        trauma = 0.0f;
    }

    PIP3D_FORCE_INLINE void addExplosion() { addTrauma(1.00f); }
    PIP3D_FORCE_INLINE void addDamage() { addTrauma(0.55f); }
    PIP3D_FORCE_INLINE void addStep() { addTrauma(0.12f); }
    PIP3D_FORCE_INLINE void addLanding() { addTrauma(0.35f); }

    PIP3D_FORCE_INLINE void clear()
    {
      trauma = 0.0f;
      timePhase = 0.0f;
    }

    PIP3D_FORCE_INLINE void update(float dt)
    {
      if (trauma > 0.0f)
      {
        trauma -= decay * dt;
        if (trauma < 0.0f)
          trauma = 0.0f;
        timePhase += dt * frequency;
        if (timePhase > 628.318f)
          timePhase -= 628.318f;
      }
      else
      {
        timePhase = 0.0f;
      }
    }

    PIP3D_FORCE_INLINE bool isActive() const { return trauma > 0.0f; }

    PIP3D_FORCE_INLINE void getOffsets(float &outPitch, float &outYaw,
                                       float &outRoll, Vector3 &outOffset) const
    {
      if (trauma <= 0.0f)
      {
        outPitch = outYaw = outRoll = 0.0f;
        outOffset.x = outOffset.y = outOffset.z = 0.0f;
        return;
      }

      const float shakePos = trauma * trauma;
      const float shakeRot = shakePos * FastMath::fastInvSqrt(trauma);

      const float s1 = FastMath::fastSin(timePhase);
      const float s2 = FastMath::fastSin(timePhase * 1.618f);
      const float s3 = FastMath::fastSin(timePhase * 2.236f);
      const float s4 = FastMath::fastSin(timePhase * 3.141f);

      outPitch = maxPitch * shakeRot * (s1 * 0.5f + s2 * 0.5f);
      outYaw = maxYaw * shakeRot * (s3 * 0.5f + s4 * 0.5f);
      outRoll = maxRoll * shakeRot * (s1 * 0.3f + s4 * 0.7f);

      outOffset.x = maxOffsetX * shakePos * s2;
      outOffset.y = maxOffsetY * shakePos * s3;
      outOffset.z = maxOffsetZ * shakePos * s1;
    }
  };
}

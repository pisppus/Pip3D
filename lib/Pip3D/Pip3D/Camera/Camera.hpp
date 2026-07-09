#pragma once

#include <algorithm>
#include <cmath>

#include "Animation.hpp"
#include "Frustum.hpp"
#include "Math/Algebra.hpp"
#include "Shake.hpp"

namespace pip3D
{
  enum ProjectionType
  {
    PERSPECTIVE,
    ORTHOGRAPHIC
  };

  struct Camera
  {
    Vector3 position;
    Vector3 target;
    Vector3 up;

    ProjectionType projectionType = PERSPECTIVE;

    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    float orthoWidth = 10.0f;
    float orthoHeight = 10.0f;

    CameraAnimation anim;
    CameraShake shakeState;

    static constexpr float kAspectEps = 1e-6f;

    struct Cache
    {
      uint8_t flags = CameraDirty::ALL;
      float lastAspect = 0.0f;
      float halfW = 0.0f;
      float halfH = 0.0f;
      Vector3 cachedForward;
      Vector3 cachedRight;
      Matrix4x4 view;
      Matrix4x4 proj;
      Matrix4x4 viewProj;
    };
    mutable Cache cache;

    Camera(const Vector3 &pos = Vector3(0, 0, -5),
           const Vector3 &tgt = Vector3(0, 0, 0),
           const Vector3 &upVec = Vector3(0, 1, 0))
        : position(pos), target(tgt), up(upVec)
    {
      up.normalize();
    }

    PIP3D_FORCE_INLINE const Matrix4x4 &getViewMatrix() const
    {
      if (cache.flags & CameraDirty::VIEW)
      {
        recomputeView();
        cache.flags |= CameraDirty::VP;
      }
      return cache.view;
    }

    PIP3D_FORCE_INLINE const Matrix4x4 &getProjectionMatrix(float aspect) const
    {
      if (unlikely(cache.flags & CameraDirty::PROJ) ||
          fabsf(aspect - cache.lastAspect) > kAspectEps)
      {
        recomputeProj(aspect);
        cache.flags |= CameraDirty::VP;
      }
      return cache.proj;
    }

    PIP3D_FORCE_INLINE const Matrix4x4 &getViewProjectionMatrix(float aspect) const
    {
      uint8_t f = cache.flags;
      if (f & CameraDirty::VP)
      {
        if (f & CameraDirty::VIEW)
          recomputeView();
        if (unlikely((f & CameraDirty::PROJ) ||
                     fabsf(aspect - cache.lastAspect) > kAspectEps))
          recomputeProj(aspect);
        cache.viewProj = cache.proj * cache.view;
        cache.flags &= ~CameraDirty::VP;
      }
      return cache.viewProj;
    }

    PIP3D_FORCE_INLINE const Vector3 &forward() const
    {
      if (cache.flags & CameraDirty::VECTORS)
        updateVectors();
      return cache.cachedForward;
    }
    PIP3D_FORCE_INLINE const Vector3 &right() const
    {
      if (cache.flags & CameraDirty::VECTORS)
        updateVectors();
      return cache.cachedRight;
    }
    PIP3D_FORCE_INLINE const Vector3 &upVec() const { return up; }

    PIP3D_FORCE_INLINE void move(float fwdAmt, float rightAmt, float upAmt)
    {
      if (fwdAmt == 0.0f && rightAmt == 0.0f && upAmt == 0.0f)
        return;

      Vector3 delta;
      if (fwdAmt != 0.0f || rightAmt != 0.0f)
      {
        if (cache.flags & CameraDirty::VECTORS)
          updateVectors();
        delta = cache.cachedForward * fwdAmt + cache.cachedRight * rightAmt;
        if (upAmt != 0.0f)
          delta += up * upAmt;
      }
      else
      {
        delta = up * upAmt;
      }

      position += delta;
      target += delta;
      invalidateView();
    }

    PIP3D_FORCE_INLINE void moveForward(float d) { move(d, 0.0f, 0.0f); }
    PIP3D_FORCE_INLINE void moveBackward(float d) { move(-d, 0.0f, 0.0f); }
    PIP3D_FORCE_INLINE void moveRight(float d) { move(0.0f, d, 0.0f); }
    PIP3D_FORCE_INLINE void moveLeft(float d) { move(0.0f, -d, 0.0f); }
    PIP3D_FORCE_INLINE void moveUp(float d) { move(0.0f, 0.0f, d); }
    PIP3D_FORCE_INLINE void moveDown(float d) { move(0.0f, 0.0f, -d); }

    PIP3D_FORCE_INLINE void rotateDeg(float yawDeg, float pitchDeg)
    {
      rotateRad(yawDeg * kDegToRad, pitchDeg * kDegToRad);
    }

    void rotateRad(float yawRad, float pitchRad)
    {
      if (cache.flags & CameraDirty::VECTORS)
        updateVectors();

      const Vector3 &fwd = cache.cachedForward;

      float yaw = atan2f(fwd.x, fwd.z) + yawRad;
      float pitch = asinf(clamp(fwd.y, -1.0f, 1.0f)) + pitchRad;

      constexpr float kPitchLimit = 1.5533f;
      if (pitch > kPitchLimit)
        pitch = kPitchLimit;
      if (pitch < -kPitchLimit)
        pitch = -kPitchLimit;

      float sp, cp;
      FastMath::fastSinCos(pitch, sp, cp);
      float sy, cy;
      FastMath::fastSinCos(yaw, sy, cy);

      const Vector3 finalFwd(sy * cp, sp, cy * cp);

      const Vector3 d = target - position;
      const float distSq = d.lengthSquared();
      const float dist = (distSq > 1e-12f)
                             ? distSq * FastMath::fastInvSqrt(distSq)
                             : 1.0f;

      target = position + finalFwd * dist;
      invalidateView();
    }

    PIP3D_FORCE_INLINE void rollDeg(float angleDeg)
    {
      rollRad(angleDeg * kDegToRad);
    }

    void rollRad(float angleRad)
    {
      if (cache.flags & CameraDirty::VECTORS)
        updateVectors();
      Quaternion q = Quaternion::fromAxisAngle(cache.cachedForward, angleRad);
      up = q.rotate(up);
      up.normalize();
      invalidateView();
    }

    PIP3D_FORCE_INLINE void lookAt(const Vector3 &newTarget)
    {
      target = newTarget;
      invalidateView();
    }

    PIP3D_FORCE_INLINE void lookAt(const Vector3 &newTarget, const Vector3 &newUp)
    {
      target = newTarget;
      up = newUp;
      up.normalize();
      invalidateView();
    }

    void orbit(const Vector3 &center, float radius, float azimuth, float elevation)
    {
      float sinEl, cosEl, sinAz, cosAz;
      FastMath::fastSinCos(elevation, sinEl, cosEl);
      FastMath::fastSinCos(azimuth, sinAz, cosAz);

      const float rCosEl = radius * cosEl;
      const float rSinEl = radius * sinEl;

      position = Vector3(center.x + rCosEl * cosAz,
                         center.y + rSinEl,
                         center.z + rCosEl * sinAz);
      target = center;
      invalidateView();
    }

    PIP3D_FORCE_INLINE void setPerspective(float fovDegrees = 60.0f, float n = 0.1f, float f = 100.0f)
    {
      fov = fmaxf(1.0f, fminf(179.0f, fovDegrees));
      nearPlane = fmaxf(0.001f, n);
      farPlane = fmaxf(nearPlane + 0.1f, f);
      projectionType = PERSPECTIVE;
      invalidateProjection();
    }

    PIP3D_FORCE_INLINE void setOrtho(float width = 10.0f, float height = 10.0f,
                                     float n = 0.1f, float f = 100.0f)
    {
      orthoWidth = fmaxf(0.1f, width);
      orthoHeight = fmaxf(0.1f, height);
      nearPlane = fmaxf(0.001f, n);
      farPlane = fmaxf(nearPlane + 0.1f, f);
      projectionType = ORTHOGRAPHIC;
      invalidateOrtho();
    }

    PIP3D_FORCE_INLINE void animateTo(const Vector3 &newPos, const Vector3 &newTgt,
                                      float dur = 1.0f,
                                      CameraAnimation::Type t = CameraAnimation::SMOOTH)
    {
      anim.reset(position, target, up, fov,
                 newPos, newTgt, up, fov, dur, t);
    }

    PIP3D_FORCE_INLINE void animatePos(const Vector3 &newPos, float dur = 1.0f)
    {
      animateTo(newPos, target + (newPos - position), dur);
    }

    PIP3D_FORCE_INLINE void animateTarget(const Vector3 &newTgt, float dur = 1.0f)
    {
      animateTo(position, newTgt, dur);
    }

    PIP3D_FORCE_INLINE void animateFOV(float newFov, float dur = 1.0f)
    {
      anim.reset(position, target, up, fov,
                 position, target, up, newFov,
                 dur, CameraAnimation::SMOOTH);
    }

    void updateAnim(float dt)
    {
      shakeState.update(dt);
      if (shakeState.trauma > 0.0f)
        cache.flags |= CameraDirty::VIEW | CameraDirty::VP;

      if (!anim.active)
        return;

      const float st = anim.tick(dt);

      position = anim.startPos + anim.deltaPos * st;
      target = anim.startTgt + anim.deltaTgt * st;
      up = anim.startUp + anim.deltaUp * st;
      up.normalize();
      fov = anim.startFov + anim.deltaFov * st;

      cache.flags |= anim.dirtyMask;
    }

    PIP3D_FORCE_INLINE void stopAnim() { anim.stop(); }
    PIP3D_FORCE_INLINE bool isAnimating() const { return anim.active; }

    PIP3D_FORCE_INLINE void shake(float amount) { shakeState.addTrauma(amount); }
    PIP3D_FORCE_INLINE void shakeExplosion() { shakeState.addExplosion(); }
    PIP3D_FORCE_INLINE void shakeDamage() { shakeState.addDamage(); }
    PIP3D_FORCE_INLINE void shakeStep() { shakeState.addStep(); }
    PIP3D_FORCE_INLINE void shakeLanding() { shakeState.addLanding(); }

    PIP3D_FORCE_INLINE void markDirty() { cache.flags = CameraDirty::ALL; }
    PIP3D_FORCE_INLINE bool isViewDirty() const { return (cache.flags & CameraDirty::VIEW) != 0; }
    PIP3D_FORCE_INLINE bool isProjDirty() const { return (cache.flags & CameraDirty::PROJ) != 0; }

  private:
    PIP3D_FORCE_INLINE void invalidateView()
    {
      cache.flags |= CameraDirty::VIEW | CameraDirty::VP | CameraDirty::VECTORS;
    }
    PIP3D_FORCE_INLINE void invalidateProjection()
    {
      cache.flags |= CameraDirty::PROJ | CameraDirty::VP;
    }
    PIP3D_FORCE_INLINE void invalidateOrtho()
    {
      cache.flags |= CameraDirty::ORTHO;
      invalidateProjection();
    }

    PIP3D_FORCE_INLINE void recomputeView() const
    {
      if (shakeState.trauma > 0.0f)
      {
        if (cache.flags & CameraDirty::VECTORS)
          updateVectors();

        float pitch, yaw, roll;
        Vector3 offset;
        shakeState.getOffsets(pitch, yaw, roll, offset);

        Vector3 fwd = cache.cachedForward;
        const Vector3 &rightVec = cache.cachedRight;

        Vector3 effPos = position + rightVec * offset.x + up * offset.y + fwd * offset.z;
        Vector3 effUp = up;
        Vector3 effTgt;

        if (pitch != 0.0f || yaw != 0.0f || roll != 0.0f)
        {
          Quaternion qShake = Quaternion::fromEuler(pitch * kDegToRad,
                                                    yaw * kDegToRad,
                                                    roll * kDegToRad);
          fwd = qShake.rotate(fwd);
          effTgt = effPos + fwd;
          effUp = qShake.rotate(up);
          float upLen = effUp.lengthSquared();
          if (likely(upLen > 1e-12f))
            effUp *= FastMath::fastInvSqrt(upLen);
        }
        else
        {
          effTgt = effPos + fwd;
        }

        cache.view.lookAt(effPos, effTgt, effUp);
      }
      else
      {
        cache.view.lookAt(position, target, up);
      }

      cache.flags &= ~CameraDirty::VIEW;
    }

    PIP3D_FORCE_INLINE void recomputeProj(float aspect) const
    {
      switch (projectionType)
      {
      case PERSPECTIVE:
        cache.proj.setPerspective(fov, aspect, nearPlane, farPlane);
        break;
      case ORTHOGRAPHIC:
      default:
        if (cache.flags & CameraDirty::ORTHO)
        {
          cache.halfW = orthoWidth * 0.5f;
          cache.halfH = orthoHeight * 0.5f;
          cache.flags &= ~CameraDirty::ORTHO;
        }
        const float aspectFactor = fmaxf(1.0f, aspect);
        const float adjW = cache.halfW * aspectFactor;
        const float adjH = cache.halfH * FastMath::fastReciprocal(aspectFactor);
        cache.proj.setOrthographic(-adjW, adjW, -adjH, adjH, nearPlane, farPlane);
        break;
      }
      cache.flags &= ~CameraDirty::PROJ;
      cache.lastAspect = aspect;
    }

    PIP3D_FORCE_INLINE void updateVectors() const
    {
      cache.cachedForward = target - position;
      float fwdLenSq = cache.cachedForward.lengthSquared();
      if (unlikely(fwdLenSq < 1e-12f))
      {
        cache.cachedForward = Vector3(0, 0, 1);
      }
      else
      {
        cache.cachedForward *= FastMath::fastInvSqrt(fwdLenSq);
      }

      cache.cachedRight = cache.cachedForward.cross(up);
      float rightLenSq = cache.cachedRight.lengthSquared();
      if (unlikely(rightLenSq < 1e-12f))
      {
        const Vector3 altUp = (fabsf(cache.cachedForward.y) < 0.999f)
                                  ? Vector3(0, 1, 0)
                                  : Vector3(1, 0, 0);
        cache.cachedRight = cache.cachedForward.cross(altUp);
        rightLenSq = cache.cachedRight.lengthSquared();
      }
      if (likely(rightLenSq > 1e-12f))
        cache.cachedRight *= FastMath::fastInvSqrt(rightLenSq);

      cache.flags &= ~CameraDirty::VECTORS;
    }
  };

  class CameraController
  {
  public:
    PIP3D_FORCE_INLINE static Vector3 project(const Vector3 &v,
                                              const Matrix4x4 &vp,
                                              float halfWidth, float halfHeight,
                                              int16_t viewportX, int16_t viewportY)
    {
      const float *PIP3D_RESTRICT m = vp.m;
      const float clipX = m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12];
      const float clipY = m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13];
      const float clipZ = m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14];
      const float clipW = m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15];
      const float invW = FastMath::fastReciprocal(clipW);
      const float ndcX = clipX * invW;
      const float ndcY = clipY * invW;
      const float ndcZ = clipZ * invW;
      return Vector3(
          (ndcX + 1.0f) * halfWidth + static_cast<float>(viewportX),
          (1.0f - ndcY) * halfHeight + static_cast<float>(viewportY),
          (ndcZ + 1.0f) * 0.5f);
    }

    PIP3D_FORCE_INLINE static Vector3 project(const Vector3 &v,
                                              const Matrix4x4 &vp,
                                              const Viewport &viewport)
    {
      return project(v, vp,
                     static_cast<float>(viewport.width) * 0.5f,
                     static_cast<float>(viewport.height) * 0.5f,
                     viewport.x, viewport.y);
    }

    static void updateViewProjectionIfNeeded(Camera &camera,
                                             const Viewport &viewport,
                                             Matrix4x4 &viewMatrix,
                                             Matrix4x4 &projMatrix,
                                             Matrix4x4 &viewProjMatrix,
                                             CameraFrustum &frustum,
                                             bool &viewProjMatrixDirty)
    {
      if (!viewProjMatrixDirty && !camera.isViewDirty() && !camera.isProjDirty())
        return;

      const float aspect = static_cast<float>(viewport.width) * FastMath::fastReciprocal(static_cast<float>(viewport.height));
      viewMatrix = camera.getViewMatrix();
      projMatrix = camera.getProjectionMatrix(aspect);
      viewProjMatrix = projMatrix * viewMatrix;
      viewProjMatrixDirty = false;
      frustum.extractFromViewProjection(viewProjMatrix);
    }
  };
}
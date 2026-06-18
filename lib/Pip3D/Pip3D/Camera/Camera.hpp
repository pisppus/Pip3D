#pragma once

#include "Math/Algebra.hpp"
#include "Frustum.hpp"
#include "Core/Viewport.hpp"
#include <cmath>

namespace pip3D
{
  enum ProjectionType
  {
    PERSPECTIVE,
    ORTHOGRAPHIC,
    FISHEYE
  };

  struct CameraConfig
  {
    float aspectEps;

    CameraConfig() : aspectEps(1e-6f) {}
  };

  struct CameraAnimation
  {
    Vector3 startPos, startTgt, startUp, targetPos, targetTgt, targetUp;
    float startFov, targetFov, time, duration, invDuration;
    enum Type : uint8_t
    {
      LINEAR,
      SMOOTH,
      EASE
    } type;
    bool active : 1;

    CameraAnimation() : time(0), duration(1), invDuration(1.0f), type(SMOOTH), active(false) {}
  };

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

    void addTrauma(float amount)
    {
      trauma = fmaxf(0.0f, fminf(1.0f, trauma + amount));
    }

    void update(float dt)
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

    void getOffsets(float &outPitch, float &outYaw, float &outRoll, Vector3 &outOffset) const
    {
      if (trauma <= 0.0f)
      {
        outPitch = outYaw = outRoll = 0.0f;
        outOffset = Vector3(0.0f, 0.0f, 0.0f);
        return;
      }

      const float shakePos = trauma * trauma;
      const float shakeRot = trauma * sqrtf(trauma);

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

  struct Camera
  {
    mutable Vector3 position;
    mutable Vector3 target;
    mutable Vector3 up;

    ProjectionType projectionType;

    float fov;
    float nearPlane;
    float farPlane;

    float orthoWidth;
    float orthoHeight;

    float fisheyeStrength;

    CameraConfig config;
    CameraAnimation anim;
    CameraShake shakeState;

    struct Cache
    {
      Matrix4x4 view, proj, viewProj;
      Vector3 cachedForward, cachedRight;
      float lastAspect, halfW, halfH;

      Vector3 basePosition, baseTarget, baseUp;
      bool wasShakenLastFrame;

      struct
      {
        bool viewDirty : 1;
        bool projDirty : 1;
        bool vpDirty : 1;
        bool orthoDirty : 1;
        bool vectorsDirty : 1;
      } flags;

      Cache() : lastAspect(0), halfW(0), halfH(0), wasShakenLastFrame(false)
      {
        flags = {true, true, true, true, true};
      }
    };
    mutable Cache cache;

    Camera(const Vector3 &pos = Vector3(0, 0, -5),
           const Vector3 &tgt = Vector3(0, 0, 0),
           const Vector3 &upVec = Vector3(0, 1, 0))
        : position(pos), target(tgt), up(upVec), projectionType(PERSPECTIVE),
          fov(60), nearPlane(0.1f), farPlane(100), orthoWidth(10),
          orthoHeight(10), fisheyeStrength(0)
    {
      up.normalize();
      cache.basePosition = position;
      cache.baseTarget = target;
      cache.baseUp = up;
    }

    const Matrix4x4 &getViewMatrix() const
    {
      if (cache.flags.viewDirty)
      {
        cache.basePosition = position;
        cache.baseTarget = target;
        cache.baseUp = up;
        cache.wasShakenLastFrame = false;
      }
      else if (cache.wasShakenLastFrame)
      {
        position = cache.basePosition;
        target = cache.baseTarget;
        up = cache.baseUp;
        cache.wasShakenLastFrame = false;
      }

      if (cache.flags.viewDirty || shakeState.trauma > 0.0f)
      {
        if (shakeState.trauma > 0.0f)
        {
          cache.wasShakenLastFrame = true;

          float pitch, yaw, roll;
          Vector3 offset;
          shakeState.getOffsets(pitch, yaw, roll, offset);

          Vector3 fwd = target - position;
          fwd.normalize();
          Vector3 rightVec = fwd.cross(up);
          rightVec.normalize();

          Vector3 translationOffset = rightVec * offset.x + up * offset.y + fwd * offset.z;
          position += translationOffset;

          if (pitch != 0.0f || yaw != 0.0f || roll != 0.0f)
          {
            Quaternion qShake = Quaternion::fromEuler(pitch * kDegToRad, yaw * kDegToRad, roll * kDegToRad);
            fwd = qShake.rotate(fwd);
            target = position + fwd;
            up = qShake.rotate(up);
            up.normalize();
          }
          else
          {
            target = position + fwd;
          }
        }

        cache.view.lookAt(position, target, up);

        cache.flags.viewDirty = false;
        cache.flags.vpDirty = true;
        cache.flags.vectorsDirty = true;
      }
      return cache.view;
    }

    const Matrix4x4 &getProjectionMatrix(float aspect) const
    {
      if (cache.flags.projDirty)
      {
        updateProjectionMatrix(aspect);
      }
      else
      {
        const float absAspectDiff = fabsf(aspect - cache.lastAspect);
        if (absAspectDiff > config.aspectEps)
        {
          updateProjectionMatrix(aspect);
        }
      }
      return cache.proj;
    }

    void markDirty() { setAllDirty(); }

  private:
    void setAllDirty()
    {
      cache.flags = {true, true, true, true, true};
    }

    void invalidateView()
    {
      cache.flags.viewDirty = true;
      cache.flags.vpDirty = true;
      cache.flags.vectorsDirty = true;
    }

    void invalidateProjection()
    {
      cache.flags.projDirty = true;
      cache.flags.vpDirty = true;
    }

    void invalidateOrtho()
    {
      cache.flags.orthoDirty = true;
      invalidateProjection();
    }

    void updateProjectionMatrix(float aspect) const
    {
      switch (projectionType)
      {
      case PERSPECTIVE:
        cache.proj.setPerspective(fov, aspect, nearPlane, farPlane);
        break;
      case FISHEYE:
        setFisheyeProjection(aspect);
        break;
      case ORTHOGRAPHIC:
      default:
        if (cache.flags.orthoDirty)
        {
          cache.halfW = orthoWidth * 0.5f;
          cache.halfH = orthoHeight * 0.5f;
          cache.flags.orthoDirty = false;
        }
        {
          const float aspectFactor = fmaxf(1.0f, aspect);
          const float adjW = cache.halfW * aspectFactor;
          const float adjH = cache.halfH / aspectFactor;
          cache.proj.setOrthographic(-adjW, adjW, -adjH, adjH, nearPlane, farPlane);
        }
        break;
      }
      cache.flags.projDirty = false;
      cache.lastAspect = aspect;
      cache.flags.vpDirty = true;
    }

  public:
    void setPerspective(float fovDegrees = 60, float near = 0.1f,
                        float far = 100)
    {
      fov = fmaxf(1, fminf(179, fovDegrees));
      nearPlane = fmaxf(0.001f, near);
      farPlane = fmaxf(nearPlane + 0.1f, far);
      projectionType = PERSPECTIVE;
      fisheyeStrength = 0;
      invalidateProjection();
    }

    void setOrtho(float width = 10, float height = 10, float near = 0.1f,
                  float far = 100)
    {
      orthoWidth = fmaxf(0.1f, width);
      orthoHeight = fmaxf(0.1f, height);
      nearPlane = fmaxf(0.001f, near);
      farPlane = fmaxf(nearPlane + 0.1f, far);
      projectionType = ORTHOGRAPHIC;
      fisheyeStrength = 0;
      invalidateOrtho();
    }

    void setFisheye(float fovDegrees = 120, float strength = 1, float near = 0.1f,
                    float far = 100)
    {
      fov = fmaxf(10, fminf(359, fovDegrees));
      fisheyeStrength = fmaxf(0, fminf(1, strength));
      nearPlane = fmaxf(0.001f, near);
      farPlane = fmaxf(nearPlane + 0.1f, far);
      projectionType = FISHEYE;
      invalidateProjection();
    }

  private:
    void updateVectors() const
    {
      if (cache.flags.vectorsDirty)
      {
        cache.cachedForward = target - position;
        if (cache.cachedForward.length() < 1e-6f)
        {
          cache.cachedForward = Vector3(0, 0, 1);
        }
        else
        {
          cache.cachedForward.normalize();
        }

        cache.cachedRight = cache.cachedForward.cross(up);
        if (cache.cachedRight.length() < 1e-6f)
        {
          const Vector3 altUp = (fabsf(cache.cachedForward.y) < 0.999f)
                                    ? Vector3(0, 1, 0)
                                    : Vector3(1, 0, 0);
          cache.cachedRight = cache.cachedForward.cross(altUp);
        }
        cache.cachedRight.normalize();

        cache.flags.vectorsDirty = false;
      }
    }

  public:
    __attribute__((always_inline)) inline const Vector3 &forward() const
    {
      if (cache.flags.vectorsDirty)
        updateVectors();
      return cache.cachedForward;
    }
    __attribute__((always_inline)) inline const Vector3 &right() const
    {
      if (cache.flags.vectorsDirty)
        updateVectors();
      return cache.cachedRight;
    }
    const Vector3 &upVec() const { return up; }

    void move(float forwardAmount, float rightAmount, float upAmount)
    {
      if (forwardAmount == 0.0f && rightAmount == 0.0f && upAmount == 0.0f)
        return;

      Vector3 delta;
      if (forwardAmount != 0.0f || rightAmount != 0.0f)
      {
        if (cache.flags.vectorsDirty)
          updateVectors();
        delta = cache.cachedForward * forwardAmount + cache.cachedRight * rightAmount;
        if (upAmount != 0.0f)
          delta += up * upAmount;
      }
      else
      {
        delta = up * upAmount;
      }

      position += delta;
      target += delta;
      invalidateView();
    }

    void moveForward(float distance) { move(distance, 0, 0); }
    void moveBackward(float distance) { move(-distance, 0, 0); }
    void moveRight(float distance) { move(0, distance, 0); }
    void moveLeft(float distance) { move(0, -distance, 0); }
    void moveUp(float distance) { move(0, 0, distance); }
    void moveDown(float distance) { move(0, 0, -distance); }

    void rotate(float yaw, float pitch, bool degrees = true)
    {
      if (degrees)
      {
        rotateDeg(yaw, pitch);
      }
      else
      {
        rotateRad(yaw, pitch);
      }
    }

    void rotateDeg(float yawDegrees, float pitchDegrees)
    {
      rotateRad(yawDegrees * kDegToRad, pitchDegrees * kDegToRad);
    }

    void rotateRad(float yawRad, float pitchRad)
    {
      if (cache.flags.vectorsDirty)
        updateVectors();

      Quaternion pitchQ = Quaternion::fromAxisAngle(cache.cachedRight, pitchRad);
      Vector3 tempFwd = pitchQ.rotate(cache.cachedForward);

      Quaternion yawQ = Quaternion::fromAxisAngle(Vector3(0, 1, 0), yawRad);
      Vector3 finalFwd = yawQ.rotate(tempFwd);
      finalFwd.normalize();

      const float dist = (target - position).length();
      target = position + finalFwd * dist;
      invalidateView();
    }

    void lookAt(const Vector3 &newTarget)
    {
      target = newTarget;
      invalidateView();
    }

    void lookAt(const Vector3 &newTarget, const Vector3 &newUp)
    {
      target = newTarget;
      up = newUp;
      up.normalize();
      invalidateView();
    }

    void orbit(const Vector3 &center, float radius, float azimuth,
               float elevation, bool degrees = true)
    {
      const float az = degrees ? azimuth * kDegToRad : azimuth;
      const float el = degrees ? elevation * kDegToRad : elevation;
      const float cosEl = cosf(el);

      position = Vector3(center.x + radius * cosEl * cosf(az),
                         center.y + radius * sinf(el),
                         center.z + radius * cosEl * sinf(az));
      target = center;
      invalidateView();
    }

    const Matrix4x4 &getViewProjectionMatrix(float aspect) const
    {
      if (cache.flags.vpDirty)
      {
        const Matrix4x4 &view = getViewMatrix();
        const Matrix4x4 &proj = getProjectionMatrix(aspect);
        cache.viewProj = proj * view;
        cache.flags.vpDirty = false;
      }
      return cache.viewProj;
    }

    void animateTo(const Vector3 &newPos, const Vector3 &newTgt, float duration = 1.0f,
                   CameraAnimation::Type type = CameraAnimation::SMOOTH)
    {
      initAnimation(newPos, newTgt, up, fov, duration, type);
    }

    void animatePos(const Vector3 &newPos, float duration = 1.0f)
    {
      animateTo(newPos, target + (newPos - position), duration);
    }

    void animateTarget(const Vector3 &newTgt, float duration = 1.0f)
    {
      animateTo(position, newTgt, duration);
    }

    void animateFOV(float newFov, float duration = 1.0f)
    {
      initAnimation(position, target, up, newFov, duration, CameraAnimation::SMOOTH);
    }

  private:
    void initAnimation(const Vector3 &targetPos, const Vector3 &targetTgt,
                       const Vector3 &targetUp, float targetFov,
                       float duration, CameraAnimation::Type type)
    {
      anim.startPos = position;
      anim.startTgt = target;
      anim.startUp = up;
      anim.startFov = fov;
      anim.targetPos = targetPos;
      anim.targetTgt = targetTgt;
      anim.targetUp = targetUp;
      anim.targetFov = targetFov;
      anim.duration = duration;
      anim.invDuration = (duration > 0.0f) ? (1.0f / duration) : 0.0f;
      anim.time = 0;
      anim.type = type;
      anim.active = true;
    }

  public:
    void updateAnim(float deltaTime)
    {
      shakeState.update(deltaTime);
      if (shakeState.trauma > 0.0f)
      {
        markDirty();
      }

      if (!anim.active)
        return;

      anim.time += deltaTime;
      float t = (anim.duration > 0.0f) ? fminf(anim.time * anim.invDuration, 1.0f) : 1.0f;
      if (t >= 1.0f)
        anim.active = false;

      float st = t;
      switch (anim.type)
      {
      case CameraAnimation::LINEAR:
        break;
      case CameraAnimation::SMOOTH:
        st = t * t * (3 - 2 * t);
        break;
      case CameraAnimation::EASE:
        st = t < 0.5f ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t);
        break;
      default:
        break;
      }

      position = anim.startPos + (anim.targetPos - anim.startPos) * st;
      target = anim.startTgt + (anim.targetTgt - anim.startTgt) * st;
      up = anim.startUp + (anim.targetUp - anim.startUp) * st;
      up.normalize();
      fov = anim.startFov + (anim.targetFov - anim.startFov) * st;

      markDirty();
    }

    void stopAnim() { anim.active = false; }
    bool isAnimating() const { return anim.active; }

    void shake(float amount)
    {
      shakeState.addTrauma(amount);
    }

    void shakeExplosion() { shake(1.0f); }
    void shakeDamage() { shake(0.55f); }
    void shakeStep() { shake(0.12f); }
    void shakeLanding() { shake(0.35f); }

  private:
    void setFisheyeProjection(float aspect) const
    {
      float fovRad = fov * kDegToRad;
      float f = 1.0f / tanf(fovRad * 0.5f);

      cache.proj.identity();
      cache.proj.m[0] = f / aspect;
      cache.proj.m[5] = f;
      cache.proj.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
      cache.proj.m[11] = -1.0f;
      cache.proj.m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
      cache.proj.m[15] = 0.0f;

      float factor = 1.0f + fisheyeStrength * 0.5f;
      cache.proj.m[0] *= factor;
      cache.proj.m[5] *= factor;
    }
  };

  class FreeCam : public Camera
  {
  public:
    float rotSpeed = 90.0f, moveSpeed = 5.0f;

    FreeCam(const Vector3 &pos = Vector3(0, 0, -5))
        : Camera(pos, pos + Vector3(0, 0, 1)) {}

    void handleJoystick(float joyX, float joyY, float deltaTime)
    {
      if (fabsf(joyX) > 0.1f || fabsf(joyY) > 0.1f)
      {
        rotate(joyX * rotSpeed * deltaTime, joyY * rotSpeed * deltaTime);
      }
    }

    void handleButtons(bool fwd, bool back, bool left, bool right, bool up,
                       bool down, float deltaTime)
    {
      const float spd = moveSpeed * deltaTime;
      move(fwd ? spd : (back ? -spd : 0), right ? spd : (left ? -spd : 0),
           up ? spd : (down ? -spd : 0));
    }

    void handleDPad(int8_t dirX, int8_t dirY, float deltaTime)
    {
      const float spd = moveSpeed * deltaTime;
      move(dirY * spd, dirX * spd, 0);
    }

    void handleRotateButtons(bool rotLeft, bool rotRight, bool rotUp,
                             bool rotDown, float deltaTime)
    {
      const float rotSpd = rotSpeed * deltaTime;
      if (rotLeft || rotRight)
        rotate((rotRight ? 1 : -1) * rotSpd, 0);
      if (rotUp || rotDown)
        rotate(0, (rotDown ? 1 : -1) * rotSpd);
    }
  };

  class OrbitCam : public Camera
  {
  public:
    Vector3 center = Vector3(0, 0, 0);
    float radius = 10.0f, azimuth = 0, elevation = 0, zoomSpd = 1.0f,
          rotSpd = 90.0f;

    OrbitCam(const Vector3 &c = Vector3(0, 0, 0), float r = 10.0f)
        : center(c), radius(r)
    {
      updatePos();
    }

    void setCenter(const Vector3 &c)
    {
      center = c;
      updatePos();
    }
    void zoom(float delta)
    {
      radius = fmaxf(0.1f, radius + delta * zoomSpd);
      updatePos();
    }

    void handleJoystick(float joyX, float joyY, float deltaTime)
    {
      if (fabsf(joyX) > 0.1f || fabsf(joyY) > 0.1f)
      {
        const float radSpeed = rotSpd * deltaTime * kDegToRad;
        azimuth += joyX * radSpeed;
        const float halfPi = 1.57079632679489661923f;
        elevation = fmaxf(-halfPi + 0.1f,
                          fminf(halfPi - 0.1f,
                                elevation + joyY * radSpeed));
        updatePos();
      }
    }

    void handleButtons(bool zoomIn, bool zoomOut, float deltaTime)
    {
      if (zoomIn)
        zoom(-zoomSpd * deltaTime);
      if (zoomOut)
        zoom(zoomSpd * deltaTime);
    }

  private:
    void updatePos() { orbit(center, radius, azimuth, elevation, false); }
  };

  class CameraBuilder
  {
    Camera cam;

  public:
    CameraBuilder &at(const Vector3 &pos)
    {
      cam.position = pos;
      return *this;
    }
    CameraBuilder &lookAt(const Vector3 &tgt)
    {
      cam.target = tgt;
      return *this;
    }
    CameraBuilder &withUp(const Vector3 &up)
    {
      cam.up = up;
      cam.up.normalize();
      return *this;
    }
    CameraBuilder &persp(float fov = 60, float near = 0.1f, float far = 100)
    {
      cam.setPerspective(fov, near, far);
      return *this;
    }
    CameraBuilder &ortho(float w = 10, float h = 10, float near = 0.1f,
                         float far = 100)
    {
      cam.setOrtho(w, h, near, far);
      return *this;
    }
    CameraBuilder &fisheye(float fov = 120, float str = 1, float near = 0.1f,
                           float far = 100)
    {
      cam.setFisheye(fov, str, near, far);
      return *this;
    }
    CameraBuilder &withConfig(const CameraConfig &cfg)
    {
      cam.config = cfg;
      return *this;
    }
    Camera build()
    {
      cam.markDirty();
      return cam;
    }
  };

  class CameraController
  {
  public:
    static Vector3 project(const Vector3 &v,
                           const Matrix4x4 &viewProjMatrix,
                           float halfWidth,
                           float halfHeight,
                           int16_t viewportX,
                           int16_t viewportY)
    {
      const float clipX = viewProjMatrix.m[0] * v.x + viewProjMatrix.m[4] * v.y + viewProjMatrix.m[8] * v.z + viewProjMatrix.m[12];
      const float clipY = viewProjMatrix.m[1] * v.x + viewProjMatrix.m[5] * v.y + viewProjMatrix.m[9] * v.z + viewProjMatrix.m[13];
      const float clipZ = viewProjMatrix.m[2] * v.x + viewProjMatrix.m[6] * v.y + viewProjMatrix.m[10] * v.z + viewProjMatrix.m[14];
      const float clipW = viewProjMatrix.m[3] * v.x + viewProjMatrix.m[7] * v.y + viewProjMatrix.m[11] * v.z + viewProjMatrix.m[15];
      const float invW = 1.0f / clipW;

      return Vector3(
          (clipX * invW + 1.0f) * halfWidth + viewportX,
          (1.0f - clipY * invW) * halfHeight + viewportY,
          (clipZ * invW + 1.0f) * 0.5f);
    }

    static void updateViewProjectionIfNeeded(Camera &camera,
                                             const Viewport &viewport,
                                             Matrix4x4 &viewMatrix,
                                             Matrix4x4 &projMatrix,
                                             Matrix4x4 &viewProjMatrix,
                                             CameraFrustum &frustum,
                                             bool &viewProjMatrixDirty,
                                             bool &cameraChangedThisFrame)
    {
      if (viewProjMatrixDirty || camera.cache.flags.viewDirty || camera.cache.flags.projDirty)
      {
        cameraChangedThisFrame = true;
        float aspect = (float)viewport.width / viewport.height;
        viewMatrix = camera.getViewMatrix();
        projMatrix = camera.getProjectionMatrix(aspect);
        viewProjMatrix = projMatrix * viewMatrix;
        viewProjMatrixDirty = false;
        frustum.extractFromViewProjection(viewProjMatrix);
      }
    }

    static Vector3 project(const Vector3 &v,
                           const Matrix4x4 &viewProjMatrix,
                           const Viewport &viewport)
    {
      return project(v,
                     viewProjMatrix,
                     static_cast<float>(viewport.width) * 0.5f,
                     static_cast<float>(viewport.height) * 0.5f,
                     viewport.x,
                     viewport.y);
    }
  };

  struct CameraHelper
  {
    static void quickSetup(Camera &cam, float fov, float nearPlane, float farPlane)
    {
      cam.setPerspective(fov, nearPlane, farPlane);
    }
  };
}
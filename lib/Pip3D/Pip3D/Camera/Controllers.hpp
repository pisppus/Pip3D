#pragma once

#include <cmath>
#include "Camera/Camera.hpp"

namespace pip3D
{
  class FreeCam : public Camera
  {
  public:
    float rotSpeed = 90.0f;
    float moveSpeed = 5.0f;

    FreeCam(const Vector3 &pos = Vector3(0, 0, -5))
        : Camera(pos, pos + Vector3(0, 0, 1)) {}

    PIP3D_FORCE_INLINE void handleJoystick(float joyX, float joyY, float dt)
    {
      if (fabsf(joyX) > 0.1f || fabsf(joyY) > 0.1f)
        rotateDeg(joyX * rotSpeed * dt, joyY * rotSpeed * dt);
    }

    PIP3D_FORCE_INLINE void handleButtons(bool fwd, bool back, bool left, bool right,
                                          bool up, bool down, float dt)
    {
      const float spd = moveSpeed * dt;
      move(fwd ? spd : (back ? -spd : 0.0f),
           right ? spd : (left ? -spd : 0.0f),
           up ? spd : (down ? -spd : 0.0f));
    }

    PIP3D_FORCE_INLINE void handleDPad(int8_t dirX, int8_t dirY, float dt)
    {
      if (dirX == 0 && dirY == 0)
        return;
      const float spd = moveSpeed * dt;
      move(dirY * spd, dirX * spd, 0.0f);
    }

    PIP3D_FORCE_INLINE void handleRotateButtons(bool rotLeft, bool rotRight,
                                                bool rotUp, bool rotDown, float dt)
    {
      const float rs = rotSpeed * dt;
      if (rotLeft || rotRight)
        rotateDeg((rotRight ? 1.0f : -1.0f) * rs, 0.0f);
      if (rotUp || rotDown)
        rotateDeg(0.0f, (rotDown ? 1.0f : -1.0f) * rs);
    }
  };

  class OrbitCam : public Camera
  {
  public:
    Vector3 center = Vector3(0, 0, 0);
    float radius = 10.0f;
    float azimuth = 0.0f;
    float elevation = 0.0f;
    float zoomSpd = 1.0f;
    float rotSpd = 90.0f;

    OrbitCam(const Vector3 &c = Vector3(0, 0, 0), float r = 10.0f)
        : center(c), radius(r)
    {
      updatePos();
    }

    PIP3D_FORCE_INLINE void setCenter(const Vector3 &c)
    {
      center = c;
      updatePos();
    }

    PIP3D_FORCE_INLINE void zoom(float delta)
    {
      radius = fmaxf(0.1f, radius + delta * zoomSpd);
      updatePos();
    }

    PIP3D_FORCE_INLINE void handleJoystick(float joyX, float joyY, float dt)
    {
      if (fabsf(joyX) > 0.1f || fabsf(joyY) > 0.1f)
      {
        const float rs = rotSpd * dt * kDegToRad;
        azimuth += joyX * rs;
        elevation = fmaxf(-kHalfPi + 0.1f,
                          fminf(kHalfPi - 0.1f, elevation + joyY * rs));
        updatePos();
      }
    }

    PIP3D_FORCE_INLINE void handleButtons(bool zoomIn, bool zoomOut, float dt)
    {
      if (zoomIn)
        zoom(-zoomSpd * dt);
      if (zoomOut)
        zoom(zoomSpd * dt);
    }

  private:
    PIP3D_FORCE_INLINE void updatePos()
    {
      orbit(center, radius, azimuth, elevation);
    }
  };

  class CameraBuilder
  {
    Camera cam;

  public:
    PIP3D_FORCE_INLINE CameraBuilder &at(const Vector3 &pos)
    {
      cam.position = pos;
      return *this;
    }
    PIP3D_FORCE_INLINE CameraBuilder &lookAt(const Vector3 &tgt)
    {
      cam.target = tgt;
      return *this;
    }
    PIP3D_FORCE_INLINE CameraBuilder &withUp(const Vector3 &up)
    {
      cam.up = up;
      cam.up.normalize();
      return *this;
    }

    PIP3D_FORCE_INLINE CameraBuilder &persp(float fovDeg = 60.0f, float n = 0.1f, float f = 100.0f)
    {
      cam.setPerspective(fovDeg, n, f);
      return *this;
    }

    PIP3D_FORCE_INLINE Camera build()
    {
      cam.markDirty();
      return cam;
    }
  };
}

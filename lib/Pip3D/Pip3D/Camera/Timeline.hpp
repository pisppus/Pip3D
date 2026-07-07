#pragma once

#include "Camera/Camera.hpp"

namespace pip3D
{
    struct CameraKeyframe
    {
        Vector3 position;
        Vector3 target;
        float fov;
        float duration;
        CameraAnimation::Type type;
        Vector3 up;

        CameraKeyframe(const Vector3 &pos = Vector3(),
                       const Vector3 &tgt = Vector3(),
                       float fovDegrees = 60.0f,
                       float dur = 1.0f,
                       CameraAnimation::Type t = CameraAnimation::SMOOTH,
                       const Vector3 &upVec = Vector3(0.0f, 1.0f, 0.0f))
            : position(pos), target(tgt), fov(fovDegrees), duration(dur), type(t), up(upVec)
        {
        }
    };

    class CameraTimeline
    {
        const CameraKeyframe *keys;
        int8_t count;
        int8_t currentSegment;
        bool loop;
        bool playing;

    public:
        CameraTimeline()
            : keys(nullptr), count(0), currentSegment(-1), loop(false), playing(false)
        {
        }

        void setTrack(const CameraKeyframe *k, int8_t n, bool looped = false)
        {
            keys = k;
            count = n;
            loop = looped;
            currentSegment = -1;
            playing = false;
        }

        void start(Camera &cam)
        {
            if (!keys || count < 2)
            {
                if (keys && count == 1)
                    applyKey(cam, keys[0]);
                playing = false;
                return;
            }
            currentSegment = 0;
            applyKey(cam, keys[0]);
            setupAnim(cam, keys[0], keys[1]);
            playing = true;
        }

        PIP3D_FORCE_INLINE void update(Camera &cam, float dt)
        {
            if (!playing)
                return;

            cam.updateAnim(dt);
            if (!cam.isAnimating())
                advance(cam);
        }

        PIP3D_FORCE_INLINE bool isPlaying() const { return playing; }

    private:
        PIP3D_FORCE_INLINE static void applyKey(Camera &cam, const CameraKeyframe &k)
        {
            cam.position = k.position;
            cam.target = k.target;
            cam.up = k.up;
            cam.fov = k.fov;
            cam.markDirty();
        }

        PIP3D_FORCE_INLINE static void setupAnim(Camera &cam, const CameraKeyframe &from, const CameraKeyframe &to)
        {
            if (from.duration > 0.0f)
            {
                cam.anim.reset(from.position, from.target, from.up, from.fov,
                               to.position, to.target, to.up, to.fov,
                               from.duration, from.type);
            }
            else
            {
                cam.anim.active = false;
            }
        }

        PIP3D_FORCE_INLINE void advance(Camera &cam)
        {
            ++currentSegment;
            if (currentSegment >= count - 1)
            {
                if (!loop)
                {
                    playing = false;
                    return;
                }
                currentSegment = 0;
                applyKey(cam, keys[0]);
            }
            setupAnim(cam, keys[currentSegment], keys[currentSegment + 1]);
        }
    };
}

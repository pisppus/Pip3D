#pragma once

#include <cstdint>
#include <cstddef>

#include <PipCore/Platform.hpp>
#include <PipCore/Audio.hpp>
#include <PipCore/Audio/Codec/PAC.hpp>

#include "Camera/Camera.hpp"
#include "Math/Algebra.hpp"
#include "Physics/Collision/Raycast.hpp"
#include "Physics/World.hpp"
#include "Audio/Reverb.hpp"

namespace pip3D
{

    struct SpatialSource
    {
        pipcore::SoundHandle handle;
        Vector3 worldPos;
        float refDistance = 4.0f;
        float maxDistance = 40.0f;
        float rolloff = 1.0f;
        float volume = 1.0f;
        bool active = false;
    };

    struct ReverbProbe
    {
        static constexpr uint8_t N_RAYS = 10;

        float distances[N_RAYS] = {};
        bool hits[N_RAYS] = {};
        uint8_t hitCount = 0;
        uint16_t t60_ms = 0;
        bool roomClosed = false;
        float avgDist = 0.0f;
        float nearestWall = 0.0f;
    };

    class SpatialAudio
    {
    public:
        static constexpr float kMinBleed = 0.10f;
        static constexpr float kBackAttenuation = 0.15f;
        static constexpr float kHalfPi = 1.57079632679489661923f;

        SpatialAudio() = default;

        void bindPhysics(PhysicsWorld *w) noexcept { _physics = w; }

        void enableReverb(bool on) noexcept
        {
            _reverbEnabled = on;
            _reverb.setEnabled(on);
        }

        void setReverbAbsorption(float alpha) noexcept
        {
            _reverbAlpha = clamp(alpha, 0.01f, 1.0f);
        }

        void setReverbWet(float wet01) noexcept { _reverb.setWet(wet01); }

        void attachToAudio(pipcore::Audio &a) noexcept
        {
            _audio = &a;
            a.setMixHook(&SpatialAudio::mixHookTrampoline, &_reverb);
        }

        void update(const Camera &camera) noexcept
        {
            pipcore::Audio *a = getAudio();
            if (!a || !a->ready())
                return;
            a->update();

            for (size_t i = 0; i < MAX_SPATIAL_SOURCES; ++i)
            {
                SpatialSource &s = _sources[i];
                if (!s.active)
                    continue;
                uint16_t gL = 0, gR = 0;
                computeGains(camera, s.worldPos, s.refDistance, s.maxDistance,
                             s.rolloff, s.volume, gL, gR);
                a->setVoiceGains(s.handle, gL, gR);
            }

            ++_frameCounter;
            if ((_frameCounter % kReverbUpdatePeriod) == 0)
            {
                castRays(camera.position);
                computeRoomProperties();
                pushReverbProfile();
            }
        }

        pipcore::SoundHandle play3D(
            const uint8_t *pacData, size_t pacSize,
            const Camera &camera, const Vector3 &worldPos,
            float refDistance = 4.0f, float maxDistance = 40.0f,
            float rolloff = 1.0f, float volume = 1.0f,
            pipcore::Bus bus = pipcore::Bus::SFX, uint8_t priority = 5) noexcept
        {
            pipcore::Audio *a = getAudio();
            if (!a)
                return {};

            uint16_t gL = 0, gR = 0;
            computeGains(camera, worldPos, refDistance, maxDistance, rolloff, volume, gL, gR);

            pipcore::SoundHandle h = a->play(pacData, pacSize, 1.0f, gL, gR, bus, priority);

            if (h.id != 0)
            {
                for (size_t i = 0; i < MAX_SPATIAL_SOURCES; ++i)
                {
                    if (!_sources[i].active)
                    {
                        _sources[i].handle = h;
                        _sources[i].worldPos = worldPos;
                        _sources[i].refDistance = refDistance;
                        _sources[i].maxDistance = maxDistance;
                        _sources[i].rolloff = rolloff;
                        _sources[i].volume = volume;
                        _sources[i].active = true;
                        break;
                    }
                }
            }
            return h;
        }

        void setPosition(pipcore::SoundHandle h, const Vector3 &pos) noexcept
        {
            if (SpatialSource *s = findSource(h))
                s->worldPos = pos;
        }

        void stop(pipcore::SoundHandle h) noexcept
        {
            if (h.id == 0)
                return;
            if (pipcore::Audio *a = getAudio())
                a->stop(h);
            if (SpatialSource *s = findSource(h))
                s->active = false;
        }

        [[nodiscard]] const ReverbProbe &probe() const noexcept { return _probe; }
        [[nodiscard]] const Reverb &reverb() const noexcept { return _reverb; }
        [[nodiscard]] bool reverbEnabled() const noexcept { return _reverbEnabled; }
        [[nodiscard]] float absorption() const noexcept { return _reverbAlpha; }

    private:
        static constexpr size_t MAX_SPATIAL_SOURCES = 16;
        static constexpr Vector3 kRayDirs[ReverbProbe::N_RAYS] = {
            Vector3(1.0f, 0.0f, 0.0f),
            Vector3(-1.0f, 0.0f, 0.0f),
            Vector3(0.0f, 0.0f, 1.0f),
            Vector3(0.0f, 0.0f, -1.0f),
            Vector3(0.70710678f, 0.0f, 0.70710678f),
            Vector3(-0.70710678f, 0.0f, 0.70710678f),
            Vector3(0.70710678f, 0.0f, -0.70710678f),
            Vector3(-0.70710678f, 0.0f, -0.70710678f),
            Vector3(0.0f, 1.0f, 0.0f),
            Vector3(0.0f, -1.0f, 0.0f),
        };

        static constexpr uint8_t kCardinalIdx[6] = {0, 1, 8, 9, 2, 3};

        static constexpr float kSpeedOfSound = 343.0f;
        static constexpr float kFdnRate = 22050.0f / 2.0f;
        static constexpr float kSabineK = 0.161f;
        static constexpr float kRayMaxDist = 60.0f;
        static constexpr uint32_t kReverbUpdatePeriod = 6;

        SpatialSource _sources[MAX_SPATIAL_SOURCES] = {};
        PhysicsWorld *_physics = nullptr;
        pipcore::Audio *_audio = nullptr;
        ReverbProbe _probe;
        Reverb _reverb;
        float _reverbAlpha = 0.20f;
        bool _reverbEnabled = true;
        uint32_t _frameCounter = 0;

        pipcore::Audio *getAudio() noexcept
        {
            if (_audio)
                return _audio;
            pipcore::Platform *p = pipcore::GetPlatform();
            return p ? p->audio() : nullptr;
        }

        SpatialSource *findSource(pipcore::SoundHandle h) noexcept
        {
            if (h.id == 0)
                return nullptr;
            for (size_t i = 0; i < MAX_SPATIAL_SOURCES; ++i)
                if (_sources[i].active && _sources[i].handle.id == h.id)
                    return &_sources[i];
            return nullptr;
        }

        PIP3D_FORCE_INLINE void computeGains(
            const Camera &cam, const Vector3 &pos,
            float refDist, float maxDist, float rolloff, float volume,
            uint16_t &outGainL_q15, uint16_t &outGainR_q15) noexcept
        {
            const Vector3 toSrc = pos - cam.position;
            const float distSq = toSrc.lengthSquared();

            if (distSq <= 1e-8f)
            {
                const uint16_t v_q15 = static_cast<uint16_t>(clamp(volume, 0.0f, 1.0f) * 32767.0f);
                outGainL_q15 = v_q15;
                outGainR_q15 = v_q15;
                return;
            }

            const float invDist = FastMath::fastInvSqrt(distSq);
            const float dist = distSq * invDist;
            const Vector3 dir = toSrc * invDist;

            if (dist > maxDist)
            {
                outGainL_q15 = 0;
                outGainR_q15 = 0;
                return;
            }

            float att = refDist / (refDist + rolloff * fmaxf(0.0f, dist - refDist));
            att = clamp(att, 0.0f, 1.0f);

            const float fwdDot = dir.dot(cam.forward());
            att *= 1.0f - kBackAttenuation * (1.0f - fwdDot);

            const float pan = clamp(dir.dot(cam.right()), -1.0f, 1.0f);
            const float angle = (pan * 0.5f + 0.5f) * kHalfPi;
            float sinA, cosA;
            FastMath::fastSinCos(angle, sinA, cosA);

            const float rawGainL = cosA * (1.0f - kMinBleed) + kMinBleed;
            const float rawGainR = sinA * (1.0f - kMinBleed) + kMinBleed;

            outGainL_q15 = static_cast<uint16_t>(clamp(rawGainL * att * volume, 0.0f, 1.0f) * 32767.0f);
            outGainR_q15 = static_cast<uint16_t>(clamp(rawGainR * att * volume, 0.0f, 1.0f) * 32767.0f);
        }

        void castRays(const Vector3 &origin) noexcept
        {
            _probe.hitCount = 0;
            _probe.avgDist = 0.0f;
            _probe.nearestWall = kRayMaxDist;
            for (uint8_t i = 0; i < ReverbProbe::N_RAYS; ++i)
            {
                _probe.hits[i] = false;
                _probe.distances[i] = 0.0f;
            }
            if (!_physics)
                return;

            for (uint8_t i = 0; i < ReverbProbe::N_RAYS; ++i)
            {
                Ray ray(origin, kRayDirs[i]);
                RaycastHit hit;
                if (_physics->raycast(ray, hit, kRayMaxDist) && hit.hit)
                {
                    _probe.hits[i] = true;
                    _probe.distances[i] = hit.distance;
                    _probe.avgDist += hit.distance;
                    if (hit.distance < _probe.nearestWall)
                        _probe.nearestWall = hit.distance;
                    _probe.hitCount++;
                }
            }
            if (_probe.hitCount > 0)
                _probe.avgDist /= _probe.hitCount;
        }

        void computeRoomProperties() noexcept
        {

            bool allHit = true;
            for (uint8_t i = 0; i < 6; ++i)
                if (!_probe.hits[kCardinalIdx[i]])
                {
                    allHit = false;
                    break;
                }

            _probe.roomClosed = allHit;
            if (!allHit)
            {
                _probe.t60_ms = 0;
                return;
            }

            const float xp = _probe.distances[kCardinalIdx[0]];
            const float xm = _probe.distances[kCardinalIdx[1]];
            const float yp = _probe.distances[kCardinalIdx[2]];
            const float ym = _probe.distances[kCardinalIdx[3]];
            const float zp = _probe.distances[kCardinalIdx[4]];
            const float zm = _probe.distances[kCardinalIdx[5]];

            const float roomW = xp + xm;
            const float roomH = yp + ym;
            const float roomD = zp + zm;
            const float volume = roomW * roomH * roomD;
            const float surface = 2.0f * (roomW * roomH + roomH * roomD + roomW * roomD);

            if (surface > 1.0f && _reverbAlpha > 0.001f)
            {
                float t60_s = kSabineK * volume / (surface * _reverbAlpha);
                t60_s = clamp(t60_s, 0.10f, 5.00f);
                _probe.t60_ms = static_cast<uint16_t>(t60_s * 1000.0f);
            }
            else
            {
                _probe.t60_ms = 0;
            }
        }

        void pushReverbProfile() noexcept
        {
            if (!_reverbEnabled || !_probe.roomClosed || _probe.hitCount < 4)
            {

                _reverb.setEnabled(false);
                return;
            }

            ReverbProfile p;
            p.enable = true;
            p.t60_ms = _probe.t60_ms;

            const float t60 = static_cast<float>(_probe.t60_ms);
            const float t60_log = log10f(fmaxf(t60, 1.0f));
            const float t = clamp((t60_log - 2.0f) / (3.477f - 2.0f), 0.0f, 1.0f);
            const float wetEarly = 0.40f - 0.20f * t;
            const float wetLate = 0.30f + 0.25f * t;
            p.wetEarly_q15 = static_cast<uint16_t>(wetEarly * 32767.0f);
            p.wetLate_q15 = static_cast<uint16_t>(wetLate * 32767.0f);

            struct Tap
            {
                float dist;
                uint8_t rayIdx;
            };
            Tap taps[ReverbProbe::N_RAYS];
            uint8_t nTaps = 0;
            for (uint8_t i = 0; i < ReverbProbe::N_RAYS; ++i)
            {
                if (!_probe.hits[i] || _probe.distances[i] < 1.5f)
                    continue;
                taps[nTaps].dist = _probe.distances[i];
                taps[nTaps].rayIdx = i;
                ++nTaps;
            }

            for (uint8_t i = 1; i < nTaps; ++i)
            {
                Tap key = taps[i];
                int j = i - 1;
                while (j >= 0 && taps[j].dist > key.dist)
                {
                    taps[j + 1] = taps[j];
                    --j;
                }
                taps[j + 1] = key;
            }

            const uint8_t count = (nTaps < ReverbProfile::MAX_EARLY) ? nTaps : ReverbProfile::MAX_EARLY;
            p.earlyCount = count;

            for (uint8_t i = 0; i < count; ++i)
            {
                const float dist = taps[i].dist;
                const float delaySec = (2.0f * dist) / kSpeedOfSound;
                uint16_t delaySamples = static_cast<uint16_t>(delaySec * kFdnRate);
                if (delaySamples >= 1024)
                    delaySamples = 1023;
                float gain = 1.0f / sqrtf(fmaxf(dist, 1.0f));
                if (gain > 1.0f)
                    gain = 1.0f;
                gain *= 0.5f;
                p.earlyDelay[i] = delaySamples;
                p.earlyGain[i] = static_cast<int16_t>(gain * 32767.0f);
            }

            _reverb.setProfile(p);
        }

        static void mixHookTrampoline(int32_t *mixL, int32_t *mixR,
                                      size_t frames, void *user) noexcept
        {
            static_cast<Reverb *>(user)->process(mixL, mixR, frames);
        }
    };

    inline SpatialAudio spatialAudio;

    [[nodiscard]] inline pipcore::Audio *audio() noexcept
    {
        pipcore::Platform *p = pipcore::GetPlatform();
        return p ? p->audio() : nullptr;
    }
}

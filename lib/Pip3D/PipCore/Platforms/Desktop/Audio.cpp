#include <PipCore/Features.hpp>

#if PIPCORE_TARGET_DESKTOP

#include <PipCore/Platforms/Desktop/Audio.hpp>
#include <PipCore/Audio.hpp>

#include <cstdio>
#include <cstring>

namespace pipcore::desktop
{
    Audio::~Audio() { deinit(); }

    bool Audio::init(const audio::BackendConfig &cfg) noexcept
    {
        deinit();
        _sampleRate = cfg.sampleRate;

        std::printf("[AUDIO] Desktop miniaudio @ %u Hz\n", (unsigned)_sampleRate);

#if PIPCORE_DESKTOP_AUDIO_MINIAUDIO
        ma_device_config dc = ma_device_config_init(ma_device_type_playback);
        dc.playback.format = ma_format_s16;
        dc.playback.channels = 2;
        dc.sampleRate = _sampleRate;
        dc.dataCallback = &Audio::dataCallback;
        dc.pUserData = this;

        if (ma_device_init(NULL, &dc, &_device) != MA_SUCCESS)
        {
            std::printf("[AUDIO ERROR] ma_device_init failed\n");
            return false;
        }
        if (ma_device_start(&_device) != MA_SUCCESS)
        {
            std::printf("[AUDIO ERROR] ma_device_start failed\n");
            ma_device_uninit(&_device);
            return false;
        }
        _deviceStarted = true;
        std::printf("[AUDIO] miniaudio backend ready\n");
#else
        std::printf("[AUDIO] miniaudio disabled — running headless\n");
#endif

        _ready.store(true, std::memory_order_relaxed);
        return true;
    }

    void Audio::deinit() noexcept
    {
        _ready.store(false, std::memory_order_relaxed);
#if PIPCORE_DESKTOP_AUDIO_MINIAUDIO
        if (_deviceStarted)
        {
            ma_device_uninit(&_device);
            _deviceStarted = false;
        }
#endif
    }

    void Audio::pumpCallback(int16_t *out, size_t frameCount) noexcept
    {
        if (!out || frameCount == 0)
            return;
        if (!_mixer)
        {
            std::memset(out, 0, frameCount * 2 * sizeof(int16_t));
            return;
        }

        _mixer->pumpMixer(out, frameCount);
    }

    void Audio::dataCallback(ma_device *pDevice, void *pOut,
                             const void *, ma_uint32 frameCount) noexcept
    {
        auto *self = static_cast<Audio *>(pDevice->pUserData);
        if (!self)
        {
            std::memset(pOut, 0, frameCount * sizeof(int16_t) * 2);
            return;
        }
        self->pumpCallback(static_cast<int16_t *>(pOut), frameCount);
    }
}

#endif

#include <PipCore/Features.hpp>
#include <PipCore/Audio.hpp>
#include <PipCore/Audio/Codec/PAC.hpp>

#include <algorithm>
#include <cstring>

#if PIPCORE_TARGET_ESP32
#include <Arduino.h>
#endif

namespace pipcore
{
    namespace
    {
        [[nodiscard]] inline int16_t clampSat16(int32_t v) noexcept
        {
            if (v > 32767)
                return 32767;
            if (v < -32768)
                return -32768;
            return static_cast<int16_t>(v);
        }
    }

    Audio::Audio(audio::Backend &backend) noexcept
        : _backend(backend)
    {
    }

    Audio::~Audio() { end(); }

    bool Audio::configure(const SoundConfig &cfg) noexcept
    {
        end();
        _cfg = cfg;
        _configured = true;
        return true;
    }

    bool Audio::begin() noexcept
    {
        if (_ready)
            return true;
        if (!_configured)
            (void)configure(SoundConfig());

        audio::BackendConfig bcfg;
        bcfg.sampleRate = _cfg.sampleRate;
        bcfg.i2sPort = _cfg.i2sPort;
        bcfg.bck = _cfg.bck;
        bcfg.ws = _cfg.ws;
        bcfg.dataOut = _cfg.dataOut;
        if (!_backend.init(bcfg))
        {
#if PIPCORE_TARGET_ESP32
            Serial.println("[AUDIO INIT ERROR] backend init failed");
#else
            std::printf("[AUDIO INIT ERROR] backend init failed\n");
#endif
            return false;
        }

#if PIPCORE_TARGET_ESP32
        _mutex = xSemaphoreCreateMutex();
        if (!_mutex)
        {
            Serial.println("[AUDIO INIT ERROR] mutex failed");
            _backend.deinit();
            return false;
        }
#endif

        _ready = true;

#if PIPCORE_TARGET_ESP32

        const BaseType_t res = xTaskCreatePinnedToCore(
            mixerTaskEntry, "PipCoreAudio", 2048, this, 5, &_mixerTaskHandle, 0);
        if (res != pdPASS)
        {
            Serial.println("[AUDIO INIT ERROR] task create failed");
            _ready = false;
            vSemaphoreDelete(_mutex);
            _mutex = nullptr;
            _backend.deinit();
            return false;
        }
        Serial.println("[AUDIO INIT SUCCESS] PAC mixer + I2S backend ready");
#else
        std::printf("[AUDIO INIT SUCCESS] PAC mixer + desktop backend ready\n");
#endif

        return true;
    }

    void Audio::end() noexcept
    {
        _ready = false;

#if PIPCORE_TARGET_ESP32
        if (_mixerTaskHandle)
        {
            vTaskDelete(_mixerTaskHandle);
            _mixerTaskHandle = nullptr;
        }
        if (_mutex)
        {
            vSemaphoreDelete(_mutex);
            _mutex = nullptr;
        }
#endif

        _backend.deinit();

        for (uint8_t i = 0; i < MaxVoices; ++i)
            _voices[i] = Voice();
        _activeCount = 0;
    }

    void Audio::setMasterVolume(float v01) noexcept
    {
        v01 = std::clamp(v01, 0.0f, 1.0f);
        _masterVol_q15 = static_cast<uint16_t>(v01 * 32767.0f);
    }

    float Audio::masterVolume() const noexcept
    {
        return static_cast<float>(_masterVol_q15) / 32767.0f;
    }

    void Audio::setBusVolume(Bus bus, float v01) noexcept
    {
        v01 = std::clamp(v01, 0.0f, 1.0f);
#if PIPCORE_TARGET_ESP32
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            _busVol_q15[static_cast<uint8_t>(bus)] = static_cast<uint16_t>(v01 * 32767.0f);
            recomputeAllVoiceGains();
            xSemaphoreGive(_mutex);
        }
#else
        _busVol_q15[static_cast<uint8_t>(bus)] = static_cast<uint16_t>(v01 * 32767.0f);
        recomputeAllVoiceGains();
#endif
    }

    float Audio::busVolume(Bus bus) const noexcept
    {
        return static_cast<float>(_busVol_q15[static_cast<uint8_t>(bus)]) / 32767.0f;
    }

    void Audio::setBusQuota(const BusQuota &q) noexcept
    {
#if PIPCORE_TARGET_ESP32
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            _quota = q;
            xSemaphoreGive(_mutex);
        }
#else
        _quota = q;
#endif
    }

    void Audio::setMixHook(MixHook hook, void *user) noexcept
    {
#if PIPCORE_TARGET_ESP32
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            _mixHook = hook;
            _mixHookUser = user;
            xSemaphoreGive(_mutex);
        }
#else
        _mixHook = hook;
        _mixHookUser = user;
#endif
    }

    uint8_t Audio::countActiveOnBus(Bus bus) const noexcept
    {
        uint8_t n = 0;
        const uint8_t b = static_cast<uint8_t>(bus);
        for (int i = 0; i < MaxVoices; ++i)
            if (_voices[i].active && _voices[i].bus == b)
                ++n;
        return n;
    }

    int Audio::findFreeVoiceSlot(uint8_t priority, Bus bus) noexcept
    {
        const uint8_t b = static_cast<uint8_t>(bus);

        if (countActiveOnBus(bus) < _quota.maxVoices[b])
        {
            for (int i = 0; i < MaxVoices; ++i)
                if (!_voices[i].active)
                    return i;
        }

        int stealIdx = -1;
        uint8_t lowestPrio = priority;
        for (int i = 0; i < MaxVoices; ++i)
        {
            if (!_voices[i].active)
                continue;
            if (_voices[i].bus != b)
                continue;
            if (_voices[i].priority < lowestPrio)
            {
                lowestPrio = _voices[i].priority;
                stealIdx = i;
            }
        }
        return stealIdx;
    }

    SoundHandle Audio::play(
        const uint8_t *pacData, size_t pacSize,
        float volume, uint16_t gainL_q15, uint16_t gainR_q15,
        Bus bus, uint8_t priority) noexcept
    {
        if (!_ready || !pacData || pacSize < sizeof(audio::PACHeader))
            return {0};

        const auto *hdr = reinterpret_cast<const audio::PACHeader *>(pacData);
        if (!hdr->isValid())
            return {0};

#if PIPCORE_TARGET_ESP32
        if (!_mutex || xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) != pdTRUE)
            return {0};
#endif

        int slot = findFreeVoiceSlot(priority, bus);
        if (slot < 0)
        {
#if PIPCORE_TARGET_ESP32
            xSemaphoreGive(_mutex);
#endif
            return {0};
        }

        Voice &v = _voices[slot];
        v = Voice();

        v.pac.data = pacData + hdr->dataOffset;
        v.pac.dataEnd = v.pac.data + hdr->dataSize;
        v.pac.frameCount = hdr->frameCount;
        v.pac.loopStart = hdr->loopStart;
        v.pac.loopEnd = hdr->loopEnd;
        v.pac.isLoop = hdr->isLoop();

        if (hdr->nativeRate > 0)
            v.pac.srcStep = (static_cast<uint32_t>(hdr->sourceRate) << 16) / hdr->nativeRate;
        else
            v.pac.srcStep = 65536;
        v.pac.isPassthrough = (v.pac.srcStep == 65536u);

        v.pac.blockPtr = v.pac.data;
        v.pac.currentFrame = 0;
        v.pac.sampleInBlock = 0;
        v.pac.firstSample = true;
        v.pac.srcPhase = 0;
        v.pac.prevSrcSample = 0;
        v.pac.curSrcSample = 0;
        v.pac.pacPredictor = 0;
        v.pac.pacStepIndex = 0;
        v.pac.pacMode = audio::PAC_MODE_SILENCE;
        v.pac.blockPayloadBytes = 0;
        v.pac.active = true;

        v.handleId = _nextHandleId++;
        if (_nextHandleId == 0)
            _nextHandleId = 1;

        v.gainL_q15 = gainL_q15;
        v.gainR_q15 = gainR_q15;
        v.volume_q15 = static_cast<uint16_t>(std::clamp(volume, 0.0f, 1.0f) * 32767.0f);
        v.priority = priority;
        v.bus = static_cast<uint8_t>(bus);

        recomputeVoiceGains(v);
        v.active = true;
        v.paused = false;

        SoundHandle h{v.handleId};
#if PIPCORE_TARGET_ESP32
        xSemaphoreGive(_mutex);
#endif
        return h;
    }

    void Audio::stop(SoundHandle h) noexcept
    {
        if (h.id == 0 || !_ready)
            return;
#if PIPCORE_TARGET_ESP32
        if (!_mutex || xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) != pdTRUE)
            return;
#endif
        for (int i = 0; i < MaxVoices; ++i)
            if (_voices[i].handleId == h.id)
            {
                _voices[i].active = false;
                break;
            }
#if PIPCORE_TARGET_ESP32
        xSemaphoreGive(_mutex);
#endif
    }

    void Audio::pause(SoundHandle h) noexcept
    {
        if (h.id == 0 || !_ready)
            return;
#if PIPCORE_TARGET_ESP32
        if (!_mutex || xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) != pdTRUE)
            return;
#endif
        for (int i = 0; i < MaxVoices; ++i)
            if (_voices[i].handleId == h.id)
            {
                _voices[i].paused = true;
                break;
            }
#if PIPCORE_TARGET_ESP32
        xSemaphoreGive(_mutex);
#endif
    }

    void Audio::resume(SoundHandle h) noexcept
    {
        if (h.id == 0 || !_ready)
            return;
#if PIPCORE_TARGET_ESP32
        if (!_mutex || xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) != pdTRUE)
            return;
#endif
        for (int i = 0; i < MaxVoices; ++i)
            if (_voices[i].handleId == h.id)
            {
                _voices[i].paused = false;
                break;
            }
#if PIPCORE_TARGET_ESP32
        xSemaphoreGive(_mutex);
#endif
    }

    void Audio::setVoiceGains(SoundHandle h, uint16_t gainL_q15, uint16_t gainR_q15) noexcept
    {
        if (h.id == 0 || !_ready)
            return;
#if PIPCORE_TARGET_ESP32
        if (!_mutex)
            return;
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) != pdTRUE)
            return;
#endif
        for (int i = 0; i < MaxVoices; ++i)
            if (_voices[i].handleId == h.id && _voices[i].active)
            {
                _voices[i].gainL_q15 = gainL_q15;
                _voices[i].gainR_q15 = gainR_q15;
                recomputeVoiceGains(_voices[i]);
                break;
            }
#if PIPCORE_TARGET_ESP32
        xSemaphoreGive(_mutex);
#endif
    }

    void Audio::recomputeVoiceGains(Voice &v) noexcept
    {
        int32_t busVol = static_cast<int32_t>(_busVol_q15[v.bus]);
        int32_t volL = (static_cast<int32_t>(v.volume_q15) * static_cast<int32_t>(v.gainL_q15)) >> 15;
        int32_t volR = (static_cast<int32_t>(v.volume_q15) * static_cast<int32_t>(v.gainR_q15)) >> 15;
        v.effGainL_q15 = static_cast<uint16_t>((volL * busVol) >> 15);
        v.effGainR_q15 = static_cast<uint16_t>((volR * busVol) >> 15);
    }

    void Audio::recomputeAllVoiceGains() noexcept
    {
        for (int i = 0; i < MaxVoices; ++i)
            if (_voices[i].active)
                recomputeVoiceGains(_voices[i]);
    }

    void Audio::pumpMixer(int16_t *outInterleaved, size_t frames) noexcept
    {

        const size_t accumFrames = (frames < MIX_BLOCK_FRAMES) ? frames : MIX_BLOCK_FRAMES;
        std::memset(_accumBufL, 0, accumFrames * sizeof(int32_t));
        std::memset(_accumBufR, 0, accumFrames * sizeof(int32_t));

        uint8_t activeCount = 0;
        for (int i = 0; i < MaxVoices; ++i)
        {
            Voice &v = _voices[i];
            if (!v.active || v.paused)
                continue;
            ++activeCount;

            int32_t volL = static_cast<int32_t>(v.effGainL_q15);
            int32_t volR = static_cast<int32_t>(v.effGainR_q15);

            int32_t *accL = _accumBufL;
            int32_t *accR = _accumBufR;

            for (size_t f = 0; f < accumFrames; ++f)
            {
                int16_t sL = 0, sR = 0;
                v.pac.decodeMixFrame(sL, sR);
                if (!v.pac.active)
                {

                    v.active = false;
                    break;
                }
                *accL++ += (static_cast<int32_t>(sL) * volL) >> 15;
                *accR++ += (static_cast<int32_t>(sR) * volR) >> 15;
            }
        }
        _activeCount = activeCount;

        if (_mixHook)
            _mixHook(_accumBufL, _accumBufR, accumFrames, _mixHookUser);

        const int32_t masterVol = static_cast<int32_t>(_masterVol_q15);
        for (size_t f = 0; f < accumFrames; ++f)
        {
            outInterleaved[f * 2 + 0] = clampSat16((_accumBufL[f] * masterVol) >> 15);
            outInterleaved[f * 2 + 1] = clampSat16((_accumBufR[f] * masterVol) >> 15);
        }

        for (size_t f = accumFrames; f < frames; ++f)
        {
            outInterleaved[f * 2 + 0] = 0;
            outInterleaved[f * 2 + 1] = 0;
        }
    }

#if PIPCORE_TARGET_ESP32

    void Audio::mixerTaskEntry(void *arg) noexcept
    {
        static_cast<Audio *>(arg)->mixerLoop();
    }

    void Audio::mixerLoop() noexcept
    {
        while (_ready)
        {

            if (xSemaphoreTake(_mutex, portMAX_DELAY) != pdTRUE)
                continue;

            pumpMixer(_outputDmaBuf, MIX_BLOCK_FRAMES);

            xSemaphoreGive(_mutex);

            _backend.writeInterleavedS16(_outputDmaBuf, MIX_BLOCK_FRAMES);
        }
    }
#endif

}

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <PipCore/Platform.hpp>

#if PIPCORE_TARGET_ESP32
#include <esp_heap_caps.h>
#endif

#include "Math/Algebra.hpp"

namespace pip3D
{

    struct ReverbProfile
    {
        static constexpr uint8_t MAX_EARLY = 8;

        uint16_t earlyDelay[MAX_EARLY] = {50, 80, 110, 140, 170, 200, 240, 280};
        int16_t earlyGain[MAX_EARLY] = {20000, 16000, 13000, 10000, 8000, 6500, 5000, 4000};
        uint8_t earlyCount = 8;

        uint16_t t60_ms = 400;
        uint16_t wetEarly_q15 = 16384;
        uint16_t wetLate_q15 = 11469;
        bool enable = true;
    };

    class Reverb
    {
    public:
        Reverb() noexcept { init(); }
        ~Reverb() noexcept { free(); }
        Reverb(const Reverb &) = delete;
        Reverb &operator=(const Reverb &) = delete;

        void process(int32_t *mixL, int32_t *mixR, size_t frames) noexcept
        {
            if (!_profile.enable || !_inputHistory)
                return;

            const int16_t wetEarly = static_cast<int16_t>(_profile.wetEarly_q15);
            const int16_t wetLate = static_cast<int16_t>(_profile.wetLate_q15);
            const uint8_t earlyCount = _profile.earlyCount;

            for (size_t i = 0; i + 1 < frames; i += 2)
            {

                const int32_t inL = (mixL[i] + mixL[i + 1]) >> 1;
                const int32_t inR = (mixR[i] + mixR[i + 1]) >> 1;
                _dsStateL += (inL - _dsStateL) >> 1;
                _dsStateR += (inR - _dsStateR) >> 1;
                const int32_t inMono = (_dsStateL + _dsStateR) >> 1;

                _inputDcBlocker += (inMono - _inputDcBlocker) >> 6;
                const int32_t inClean = inMono - _inputDcBlocker;

                _inputHistory[_inputHistoryPos] = clampSat16(inClean);
                const uint16_t histPos = _inputHistoryPos;
                _inputHistoryPos = (_inputHistoryPos + 1u) & kInputHistoryMask;

                int32_t early = 0;
                for (uint8_t t = 0; t < earlyCount; ++t)
                {
                    int32_t delay = static_cast<int32_t>(_profile.earlyDelay[t]) + kPreDelay;
                    if (delay >= kInputHistorySize)
                        delay = kInputHistorySize - 1;
                    int32_t tapPos = static_cast<int32_t>(histPos) - delay - 1;
                    if (tapPos < 0)
                        tapPos += kInputHistorySize;
                    early += (static_cast<int32_t>(_inputHistory[tapPos]) * _profile.earlyGain[t]) >> 15;
                }

                const int32_t d0 = _fdn[0].buffer[_fdn[0].pos];
                const int32_t d1 = _fdn[1].buffer[_fdn[1].pos];
                const int32_t d2 = _fdn[2].buffer[_fdn[2].pos];
                const int32_t d3 = _fdn[3].buffer[_fdn[3].pos];

                int32_t y0 = (d0 + d1 + d2 + d3) >> 1;
                int32_t y1 = (d0 - d1 + d2 - d3) >> 1;
                int32_t y2 = (d0 + d1 - d2 - d3) >> 1;
                int32_t y3 = (d0 - d1 - d2 + d3) >> 1;

                {
                    const int32_t diff0 = y0 - _fdnLowpassState[0];
                    _fdnLowpassState[0] += (diff0 >> 1) - (diff0 >> 3);
                }
                {
                    const int32_t diff1 = y1 - _fdnLowpassState[1];
                    _fdnLowpassState[1] += (diff1 >> 1) - (diff1 >> 3);
                }
                {
                    const int32_t diff2 = y2 - _fdnLowpassState[2];
                    _fdnLowpassState[2] += (diff2 >> 1) - (diff2 >> 3);
                }
                {
                    const int32_t diff3 = y3 - _fdnLowpassState[3];
                    _fdnLowpassState[3] += (diff3 >> 1) - (diff3 >> 3);
                }

                _fdnDcBlocker[0] += (_fdnLowpassState[0] - _fdnDcBlocker[0]) >> 5;
                _fdnDcBlocker[1] += (_fdnLowpassState[1] - _fdnDcBlocker[1]) >> 5;
                _fdnDcBlocker[2] += (_fdnLowpassState[2] - _fdnDcBlocker[2]) >> 5;
                _fdnDcBlocker[3] += (_fdnLowpassState[3] - _fdnDcBlocker[3]) >> 5;

                const int32_t f0 = _fdnLowpassState[0] - _fdnDcBlocker[0];
                const int32_t f1 = _fdnLowpassState[1] - _fdnDcBlocker[1];
                const int32_t f2 = _fdnLowpassState[2] - _fdnDcBlocker[2];
                const int32_t f3 = _fdnLowpassState[3] - _fdnDcBlocker[3];

                _fdn[0].buffer[_fdn[0].pos] = clampSat16(inClean + ((f0 * _fdnFeedback_q15[0]) >> 15));
                _fdn[1].buffer[_fdn[1].pos] = clampSat16(inClean + ((f1 * _fdnFeedback_q15[1]) >> 15));
                _fdn[2].buffer[_fdn[2].pos] = clampSat16(inClean + ((f2 * _fdnFeedback_q15[2]) >> 15));
                _fdn[3].buffer[_fdn[3].pos] = clampSat16(inClean + ((f3 * _fdnFeedback_q15[3]) >> 15));

                if (++_fdn[0].pos >= _fdn[0].size)
                    _fdn[0].pos = 0;
                if (++_fdn[1].pos >= _fdn[1].size)
                    _fdn[1].pos = 0;
                if (++_fdn[2].pos >= _fdn[2].size)
                    _fdn[2].pos = 0;
                if (++_fdn[3].pos >= _fdn[3].size)
                    _fdn[3].pos = 0;

                const int32_t lateL = (d0 + d1 + d2 + d3) >> 2;
                const int32_t lateR = (d0 - d1 + d2 - d3) >> 2;

                const int32_t wetL = ((early * wetEarly) >> 15) + ((lateL * wetLate) >> 15);
                const int32_t wetR = ((early * wetEarly) >> 15) + ((lateR * wetLate) >> 15);

                const int32_t wetL0 = (_prevWetL + wetL) >> 1;
                const int32_t wetR0 = (_prevWetR + wetR) >> 1;
                _prevWetL = wetL;
                _prevWetR = wetR;

                mixL[i] += wetL0;
                mixR[i] += wetR0;
                mixL[i + 1] += wetL;
                mixR[i + 1] += wetR;
            }
        }

        void setProfile(const ReverbProfile &p) noexcept
        {
            _profile = p;
            recomputeFeedback();
        }
        [[nodiscard]] const ReverbProfile &profile() const noexcept { return _profile; }
        void setEnabled(bool on) noexcept { _profile.enable = on; }
        [[nodiscard]] bool isEnabled() const noexcept { return _profile.enable; }

        void setWet(float wet01) noexcept
        {
            wet01 = std::clamp(wet01, 0.0f, 1.0f);
            _profile.wetEarly_q15 = static_cast<uint16_t>(wet01 * 32767.0f);
            _profile.wetLate_q15 = static_cast<uint16_t>(wet01 * 32767.0f * 0.7f);
        }

    private:
        struct FDNDelay
        {
            int16_t *buffer = nullptr;
            uint16_t size = 0;
            uint16_t pos = 0;
        };

        static constexpr uint16_t kFdnLengths[4] = {277, 331, 379, 431};
        static constexpr uint16_t kInputHistorySize = 1024;
        static constexpr uint16_t kInputHistoryMask = kInputHistorySize - 1;
        static constexpr int32_t kPreDelay = 220;

        ReverbProfile _profile;
        FDNDelay _fdn[4];
        int16_t _fdnFeedback_q15[4] = {21232, 19510, 18097, 16681};
        int32_t _fdnLowpassState[4] = {};
        int32_t _fdnDcBlocker[4] = {};
        int32_t _dsStateL = 0;
        int32_t _dsStateR = 0;
        int32_t _inputDcBlocker = 0;
        int32_t _prevWetL = 0;
        int32_t _prevWetR = 0;
        int16_t *_inputHistory = nullptr;
        uint16_t _inputHistoryPos = 0;

        PIP3D_FORCE_INLINE static int16_t clampSat16(int32_t v) noexcept
        {
            return static_cast<int16_t>(std::max<int>(-32768, std::min<int>(32767, v)));
        }

        void init() noexcept
        {
            for (int i = 0; i < 4; ++i)
            {
                _fdn[i].size = kFdnLengths[i];
                _fdn[i].pos = 0;
#if PIPCORE_TARGET_ESP32
                _fdn[i].buffer = static_cast<int16_t *>(
                    heap_caps_calloc(kFdnLengths[i], sizeof(int16_t),
                                     MALLOC_CAP_INTERNAL));
#else
                _fdn[i].buffer = static_cast<int16_t *>(
                    std::calloc(kFdnLengths[i], sizeof(int16_t)));
#endif
            }
#if PIPCORE_TARGET_ESP32
            _inputHistory = static_cast<int16_t *>(
                heap_caps_calloc(kInputHistorySize, sizeof(int16_t),
                                 MALLOC_CAP_INTERNAL));
#else
            _inputHistory = static_cast<int16_t *>(
                std::calloc(kInputHistorySize, sizeof(int16_t)));
#endif
            recomputeFeedback();
        }

        void free() noexcept
        {
            for (int i = 0; i < 4; ++i)
            {
#if PIPCORE_TARGET_ESP32
                if (_fdn[i].buffer)
                    heap_caps_free(_fdn[i].buffer);
#else
                if (_fdn[i].buffer)
                    std::free(_fdn[i].buffer);
#endif
                _fdn[i].buffer = nullptr;
            }
#if PIPCORE_TARGET_ESP32
            if (_inputHistory)
                heap_caps_free(_inputHistory);
#else
            if (_inputHistory)
                std::free(_inputHistory);
#endif
            _inputHistory = nullptr;
        }

        void recomputeFeedback() noexcept
        {
            constexpr double kFdnRate = 22050.0 / 2.0;
            const double t60_samples = (static_cast<double>(_profile.t60_ms) * kFdnRate) / 1000.0;

            for (int i = 0; i < 4; ++i)
            {
                const double gain = std::pow(0.001, static_cast<double>(kFdnLengths[i]) / t60_samples);
                _fdnFeedback_q15[i] = static_cast<int16_t>(gain * 32767.0);
            }
        }
    };
}

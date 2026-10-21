#include "Rendering/Effects/Exposure.hpp"

#include <cmath>

#include "Core/Diagnostics.hpp"
#include "Core/Platform.hpp"
#include "Math/Algebra.hpp"
#include "Math/Simd.hpp"
#include "Rendering/Renderer.hpp"

namespace pip3D
{
    namespace
    {
        constexpr int kColumns = 32;
        constexpr int kCell = SCREEN_WIDTH / kColumns;

        constexpr float kSettleEv = 0.07f;

        constexpr int kAboveMean = 16;
        constexpr int kBelowMean = 48;

        constexpr float kPull = 2.0f;
        constexpr float kEase = 0.3f;

        PIP3D_FORCE_INLINE int logLuma(uint32_t p) noexcept
        {
            const uint32_t y = (((p >> 11) << 4) + (((p >> 5) & 0x3Fu) * 20u) + ((p & 0x1Fu) << 3)) >> 3;
            const uint32_t v = y + 1u;
            const uint32_t l = 31u - static_cast<uint32_t>(__builtin_clz(v));
            return static_cast<int>((l << 4) | (((v << (31u - l)) >> 27) & 15u));
        }

        void gainBand(uint16_t *px, uint32_t gain) noexcept
        {
            constexpr uint32_t kPixels = static_cast<uint32_t>(SCREEN_WIDTH) * SCREEN_BAND_HEIGHT;
#if PIP3D_SIMD
            static_assert(kPixels % 8 == 0, "the band is a whole number of 8-pixel blocks");
            Simd::gainSpan565(px, kPixels / 8, gain);
#else
            for (uint32_t i = 0; i < kPixels; ++i)
            {
                const uint32_t p = px[i];
                const uint32_t g = gain;
                uint32_t r = ((p >> 11) * g) >> 8;
                uint32_t gr = (((p >> 5) & 0x3Fu) * g) >> 8;
                uint32_t b = ((p & 0x1Fu) * g) >> 8;
                r = r > 31u ? 31u : r;
                gr = gr > 63u ? 63u : gr;
                b = b > 31u ? 31u : b;
                px[i] = static_cast<uint16_t>((r << 11) | (gr << 5) | b);
            }
#endif
        }
    }

    void Exposure::setTarget(float brightness) noexcept
    {
        target_ = clamp(brightness, 0.05f, 0.95f);
        targetLog_ = log2f(target_);
    }

    float Exposure::getBrightness() const noexcept
    {
        return exp2f(meanLog_ * (1.0f / 16.0f) - 8.0f);
    }

    void Exposure::setRange(float minGain, float maxGain) noexcept
    {
        minGain = clamp(minGain, 0.25f, 3.5f);
        maxGain = clamp(maxGain, minGain, 3.5f);
        minEv_ = log2f(minGain);
        maxEv_ = log2f(maxGain);
    }

    void Exposure::setSpeed(float toBright, float toDark) noexcept
    {
        toBright_ = toBright < 0.05f ? 0.05f : toBright;
        toDark_ = toDark < 0.05f ? 0.05f : toDark;
    }

    void Exposure::beginFrame() noexcept
    {
        frame_ = g_frameStamp;
        const uint64_t nowUs = getSystemMicros();
        const float dt = lastUs_ == 0 ? 1.0f / 60.0f : clamp(static_cast<float>(nowUs - lastUs_) * 1e-6f, 0.0f, 0.1f);
        lastUs_ = nowUs;

        if (!enabled_)
        {
            ev_ = 0.0f;
            speed_ = 0.0f;
        }
        else if (weight_ != 0)
        {
            meanLog_ = static_cast<float>(sum_) / static_cast<float>(weight_);
            float want = strength_ * (targetLog_ + 8.0f - meanLog_ * (1.0f / 16.0f));
            want = clamp(fabsf(want) < kSettleEv ? 0.0f : want, minEv_, maxEv_);
            if (snap_)
            {
                ev_ = want;
                speed_ = 0.0f;
            }
            else
            {
                const float left = want - ev_;
                const float limit = left < 0.0f ? toBright_ : toDark_;
                const float wanted = clamp(left * kPull, -limit, limit);
                speed_ += (wanted - speed_) * (dt / (kEase + dt));
                ev_ += speed_ * dt;
                if ((left > 0.0f && ev_ > want) || (left < 0.0f && ev_ < want))
                {
                    ev_ = want;
                    speed_ = 0.0f;
                }
            }
            snap_ = false;
        }
        sum_ = 0;
        weight_ = 0;

        gain_ = static_cast<uint32_t>(clamp(exp2f(ev_) * 256.0f + 0.5f, 64.0f, 900.0f));
    }

    void Exposure::apply(uint16_t *band, int bandIndex) noexcept
    {
        const int hi = static_cast<int>(meanLog_) + kAboveMean;
        const int lo = static_cast<int>(meanLog_) - kBelowMean;
        const uint32_t bandWeight = (bandIndex * 4 >= SCREEN_BAND_COUNT && bandIndex * 4 < SCREEN_BAND_COUNT * 3) ? 2u : 1u;
        const int jitterX = static_cast<int>((frame_ * 5u) % kCell);
        const int jitterY = static_cast<int>((frame_ >> 1) & 3u);
        const uint16_t *line = band + static_cast<size_t>((SCREEN_BAND_HEIGHT * (1 + 2 * (frame_ & 1u))) / 4 + jitterY - 1) * SCREEN_WIDTH;
        for (int c = 0; c < kColumns; ++c)
        {
            int l = logLuma(line[c * kCell + jitterX]);
            l = l > hi ? hi : (l < lo ? lo : l);
            const uint32_t w = bandWeight * ((c >= kColumns / 4 && c < kColumns * 3 / 4) ? 2u : 1u);
            sum_ += w * static_cast<uint32_t>(l < 0 ? 0 : l);
            weight_ += w;
        }

        if (gain_ != 256u)
        {
            gainBand(band, gain_);
        }
    }

    void Renderer::applyExposure()
    {
        if (exposure_.frame_ != g_frameStamp)
            exposure_.beginFrame();
        if (exposure_.enabled_)
            exposure_.apply(framebuffer.getBuffer(), g_bandOffsetY / SCREEN_BAND_HEIGHT);
    }
}

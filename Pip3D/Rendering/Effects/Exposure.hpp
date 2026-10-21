#pragma once

#include <cstdint>

namespace pip3D
{
    class Renderer;
    class Exposure
    {
    public:
        void setEnabled(bool on) noexcept { enabled_ = on; }
        bool isEnabled() const noexcept { return enabled_; }

        void setTarget(float brightness) noexcept;
        float getTarget() const noexcept { return target_; }

        void setStrength(float strength) noexcept { strength_ = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength); }
        float getStrength() const noexcept { return strength_; }

        void setRange(float minGain, float maxGain) noexcept;

        void setSpeed(float toBright, float toDark) noexcept;

        void snap() noexcept { snap_ = true; }

        float getGain() const noexcept { return static_cast<float>(gain_) * (1.0f / 256.0f); }

        float getBrightness() const noexcept;

    private:
        friend class Renderer;

        void beginFrame() noexcept;
        void apply(uint16_t *band, int bandIndex) noexcept;

        float target_ = 0.5f;
        float targetLog_ = -1.0f;
        float strength_ = 0.6f;
        float minEv_ = -0.737f;
        float maxEv_ = 0.848f;
        float toBright_ = 1.5f;
        float toDark_ = 0.7f;
        float ev_ = 0.0f;
        float speed_ = 0.0f;
        float meanLog_ = 112.0f;
        uint32_t gain_ = 256;
        uint32_t sum_ = 0;
        uint32_t weight_ = 0;
        uint32_t frame_ = 0;
        uint64_t lastUs_ = 0;
        bool enabled_ = true;
        bool snap_ = false;
    };
}

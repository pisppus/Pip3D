#pragma once

#include <cstddef>
#include <cstdint>

namespace pipcore
{
    enum class Bus : uint8_t
    {
        MUSIC = 0,
        SFX = 1,
        UI = 2,
        AMBIENT = 3
    };

    struct SoundHandle
    {
        uint16_t id = 0;
    };

    class Audio
    {
    public:
        [[nodiscard]] bool ready() const noexcept { return false; }

        [[nodiscard]] SoundHandle play(const uint8_t *, size_t, float = 1.0f,
                                       uint16_t = 32767, uint16_t = 32767, Bus = Bus::SFX,
                                       uint8_t = 5) noexcept
        {
            return {};
        }

        void stop(SoundHandle) noexcept {}
        void pause(SoundHandle) noexcept {}
        void resume(SoundHandle) noexcept {}
        void setVoiceGains(SoundHandle, uint16_t, uint16_t) noexcept {}

        using MixHook = void (*)(int32_t *mixL, int32_t *mixR, size_t frames, void *user);
        void setMixHook(MixHook, void *) noexcept {}
    };
}

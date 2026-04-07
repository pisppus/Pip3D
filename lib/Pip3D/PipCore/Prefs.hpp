#pragma once

#include <PipCore/Config/Features.hpp>
#include <PipCore/Platforms/Select.hpp>
#include <cstdint>

namespace pipcore
{
#if PIPCORE_ENABLE_PREFS
    [[nodiscard]] inline uint8_t prefsLoadMaxBrightnessPercent() noexcept
    {
        if (Platform *plat = GetPlatform())
            return plat->loadMaxBrightnessPercent();
        return 100;
    }

    inline void prefsStoreMaxBrightnessPercent(uint8_t percent) noexcept
    {
        if (Platform *plat = GetPlatform())
            plat->storeMaxBrightnessPercent(percent);
    }
#else
    [[nodiscard]] inline uint8_t prefsLoadMaxBrightnessPercent() noexcept = delete;
    inline void prefsStoreMaxBrightnessPercent(uint8_t percent) noexcept = delete;
#endif
}

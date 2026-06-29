#pragma once

#include <cstdint>

namespace pipcore::esp32::services
{
    class Prefs
    {
    public:
        Prefs() = default;

        [[nodiscard]] bool loadMaxBrightnessPercent(uint8_t &percent) noexcept;
        [[nodiscard]] bool storeMaxBrightnessPercent(uint8_t percent) noexcept;
    };
}
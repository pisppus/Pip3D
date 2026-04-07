#pragma once

#include <Preferences.h>
#include <cstdint>

namespace pipcore::esp32::services
{
    class Prefs
    {
    public:
        ~Prefs() noexcept;

        [[nodiscard]] bool loadMaxBrightnessPercent(uint8_t &percent) noexcept;
        [[nodiscard]] bool storeMaxBrightnessPercent(uint8_t percent) noexcept;

    private:
        [[nodiscard]] bool ensureOpen() noexcept;

    private:
        Preferences _prefs;
        bool _opened = false;
    };
}

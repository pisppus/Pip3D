#include <PipCore/Features.hpp>

#if PIPCORE_ENABLE_PREFS

#include <PipCore/Platforms/ESP32/Services/Prefs.hpp>
#include <Preferences.h>
#include <algorithm>

namespace pipcore::esp32::services
{
    bool Prefs::loadMaxBrightnessPercent(uint8_t &percent) noexcept
    {
        percent = 100;

        Preferences prefs;
        if (!prefs.begin("pipgui", true))
            return false;

        const uint16_t raw = std::min<uint16_t>(prefs.getUShort("bmax", 100), 100);
        percent = static_cast<uint8_t>(raw);

        prefs.end();
        return true;
    }

    bool Prefs::storeMaxBrightnessPercent(uint8_t percent) noexcept
    {
        percent = std::min<uint8_t>(percent, 100);

        Preferences prefs;
        if (!prefs.begin("pipgui", false))
            return false;

        prefs.putUShort("bmax", percent);

        prefs.end();
        return true;
    }
}

#endif
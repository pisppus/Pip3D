#include <PipCore/Config/Features.hpp>

#if PIPCORE_ENABLE_PREFS

#include <PipCore/Platforms/ESP32/Services/Prefs.hpp>
#include <algorithm>

namespace pipcore::esp32::services
{
    Prefs::~Prefs() noexcept
    {
        if (_opened)
            _prefs.end();
    }

    bool Prefs::ensureOpen() noexcept
    {
        if (_opened)
            return true;
        if (!_prefs.begin("pipgui", false))
            return false;
        _opened = true;
        return true;
    }

    bool Prefs::loadMaxBrightnessPercent(uint8_t &percent) noexcept
    {
        percent = 100;
        if (!ensureOpen())
            return false;

        const uint16_t raw = std::min<uint16_t>(_prefs.getUShort("bmax", 100), 100);
        percent = static_cast<uint8_t>(raw);
        return true;
    }

    bool Prefs::storeMaxBrightnessPercent(uint8_t percent) noexcept
    {
        percent = std::min<uint8_t>(percent, 100);
        if (!ensureOpen())
            return false;
        _prefs.putUShort("bmax", percent);
        return true;
    }
}

#endif

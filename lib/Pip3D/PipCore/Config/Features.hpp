#pragma once

#if __has_include(<config.hpp>)
#include <config.hpp>
#endif

#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
namespace pipcore::detail
{
    [[nodiscard]] constexpr uint16_t builtin_bswap16(uint16_t v) noexcept
    {
        return static_cast<uint16_t>((v << 8) | (v >> 8));
    }

    [[nodiscard]] constexpr uint32_t builtin_bswap32(uint32_t v) noexcept
    {
        return ((v & 0x000000FFu) << 24) |
               ((v & 0x0000FF00u) << 8) |
               ((v & 0x00FF0000u) >> 8) |
               ((v & 0xFF000000u) >> 24);
    }
}
#ifndef __builtin_bswap16
#define __builtin_bswap16 ::pipcore::detail::builtin_bswap16
#endif
#ifndef __builtin_bswap32
#define __builtin_bswap32 ::pipcore::detail::builtin_bswap32
#endif
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#define PIPCORE_PP_CAT_IMPL(a, b) a##b
#define PIPCORE_PP_CAT(a, b) PIPCORE_PP_CAT_IMPL(a, b)

#ifndef PIPCORE_PLATFORM
#define PIPCORE_PLATFORM ESP32
#endif

#ifndef PIPCORE_DISPLAY
#define PIPCORE_DISPLAY ST7789
#endif

#define PIPCORE_PLATFORM_TAG_ESP32 1
#define PIPCORE_PLATFORM_TAG_DESKTOP 2
#define PIPCORE_PLATFORM_ID(name) PIPCORE_PP_CAT(PIPCORE_PLATFORM_TAG_, name)

#define PIPCORE_DISPLAY_TAG_ST7789 1
#define PIPCORE_DISPLAY_TAG_ILI9488 2
#define PIPCORE_DISPLAY_TAG_SIMULATOR 3
#define PIPCORE_DISPLAY_ID(name) PIPCORE_PP_CAT(PIPCORE_DISPLAY_TAG_, name)

#ifndef PIPCORE_ENABLE_PREFS
#define PIPCORE_ENABLE_PREFS 0
#endif

#ifndef PIPCORE_ENABLE_WIFI
#define PIPCORE_ENABLE_WIFI 0
#endif

#ifndef PIPCORE_ENABLE_OTA
#define PIPCORE_ENABLE_OTA 0
#endif

#ifndef PIPCORE_OTA_PROJECT_URL
#define PIPCORE_OTA_PROJECT_URL ""
#endif

#if PIPCORE_ENABLE_OTA && !PIPCORE_ENABLE_WIFI
#error "PIPCORE_ENABLE_OTA requires PIPCORE_ENABLE_WIFI"
#endif

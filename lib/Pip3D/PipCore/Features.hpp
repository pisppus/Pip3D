#pragma once

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#define PIPCORE_TARGET_ESP32 1
#else
#define PIPCORE_TARGET_ESP32 0
#endif

#if PIPCORE_TARGET_DESKTOP && __has_include(<config_sim.hpp>)
#include <config_sim.hpp>
#elif __has_include(<config.hpp>)
#include <config.hpp>
#endif

#include <cstdint>
#include <cstddef>

#if defined(ESP_PLATFORM) || defined(ESP32)
#include <esp_attr.h>
#else
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#endif

#if defined(_MSC_VER)
#include <stdlib.h>

#if defined(__cplusplus) && (__cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L))
#include <type_traits>
#define PIPCORE_HAS_CONSTEXPR_EVAL 1
#else
#define PIPCORE_HAS_CONSTEXPR_EVAL 0
#endif

namespace pipcore::detail
{
    [[nodiscard]] inline constexpr uint16_t builtin_bswap16(uint16_t v) noexcept
    {
#if PIPCORE_HAS_CONSTEXPR_EVAL
        if (std::is_constant_evaluated())
        {
            return static_cast<uint16_t>((v << 8) | (v >> 8));
        }
        else
        {
            return _byteswap_ushort(v);
        }
#else
        return _byteswap_ushort(v);
#endif
    }

    [[nodiscard]] inline constexpr uint32_t builtin_bswap32(uint32_t v) noexcept
    {
#if PIPCORE_HAS_CONSTEXPR_EVAL
        if (std::is_constant_evaluated())
        {
            return ((v & 0x000000FFu) << 24) |
                   ((v & 0x0000FF00u) << 8) |
                   ((v & 0x00FF0000u) >> 8) |
                   ((v & 0xFF000000u) >> 24);
        }
        else
        {
            return _byteswap_ulong(v);
        }
#else
        return _byteswap_ulong(v);
#endif
    }
}
#undef PIPCORE_HAS_CONSTEXPR_EVAL

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
#if PIPCORE_TARGET_ESP32 && !defined(ESP32)
#define ESP32 PIPCORE_PLATFORM_TAG_ESP32
#endif
#ifndef DESKTOP
#define DESKTOP PIPCORE_PLATFORM_TAG_DESKTOP
#endif
#define PIPCORE_PLATFORM_ID(name) name

#define PIPCORE_DISPLAY_TAG_ST7789 1
#define PIPCORE_DISPLAY_TAG_ILI9488 2
#define PIPCORE_DISPLAY_TAG_SIMULATOR 3
#define PIPCORE_DISPLAY_TAG_ST7796 4
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

#ifndef PIPCORE_ENABLE_TOUCH
#define PIPCORE_ENABLE_TOUCH 0
#endif

#ifndef PIPCORE_TOUCH_SDA
#define PIPCORE_TOUCH_SDA 8
#endif
#ifndef PIPCORE_TOUCH_SCL
#define PIPCORE_TOUCH_SCL 9
#endif
#ifndef PIPCORE_TOUCH_INT
#define PIPCORE_TOUCH_INT -1
#endif
#ifndef PIPCORE_TOUCH_I2C_ADDR
#define PIPCORE_TOUCH_I2C_ADDR 0x38
#endif
#ifndef PIPCORE_TOUCH_FREQ_HZ
#define PIPCORE_TOUCH_FREQ_HZ 400000
#endif

#ifndef PIPCORE_OTA_PROJECT_URL
#define PIPCORE_OTA_PROJECT_URL ""
#endif

#if PIPCORE_ENABLE_OTA && !PIPCORE_ENABLE_WIFI
#error "PIPCORE_ENABLE_OTA requires PIPCORE_ENABLE_WIFI"
#endif

namespace pipcore::util
{
    inline void IRAM_ATTR __attribute__((always_inline)) copySwap565(uint16_t *dst, const uint16_t *src, size_t pixels) noexcept
    {
        if (pixels == 0)
            return;

        const bool canUse32 = (((reinterpret_cast<uintptr_t>(src) | reinterpret_cast<uintptr_t>(dst)) & 3U) == 0U);

        if (canUse32)
        {
            auto *dst32 = reinterpret_cast<uint32_t *>(dst);
            auto *src32 = reinterpret_cast<const uint32_t *>(src);
            size_t pairs = pixels >> 1;

            while (pairs--)
            {
                __builtin_prefetch(src32 + 8, 0, 0);
                const uint32_t p = __builtin_bswap32(*src32++);
                *dst32++ = (p >> 16) | (p << 16);
            }

            src = reinterpret_cast<const uint16_t *>(src32);
            dst = reinterpret_cast<uint16_t *>(dst32);
            pixels &= 1U;
        }

        while (pixels--)
            *dst++ = __builtin_bswap16(*src++);
    }
}
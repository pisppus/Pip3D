#pragma once

#define PIPCORE_VERSION "2.0.0"

#include <cstddef>
#include <cstdint>
#if defined(ESP_PLATFORM)
#define PIPCORE_TARGET_ESP32 1
#define PIPCORE_TARGET_DESKTOP 0
#include "sdkconfig.h"
#else
#define PIPCORE_TARGET_ESP32 0
#define PIPCORE_TARGET_DESKTOP 1
#endif
#if defined(_MSC_VER)
#include <stdlib.h>
#include <intrin.h>

#undef NEAR
#undef FAR

#ifndef __restrict__
#define __restrict__ __restrict
#endif
#ifndef __attribute__
#define __attribute__(x)
#endif

namespace pipcore::detail
{
    [[nodiscard]] inline constexpr uint16_t builtin_bswap16(uint16_t v) noexcept
    {
        return static_cast<uint16_t>((v << 8) | (v >> 8));
    }

    [[nodiscard]] inline constexpr uint32_t builtin_bswap32(uint32_t v) noexcept
    {
        return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) | ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
    }
}

#ifndef __builtin_bswap16
#define __builtin_bswap16 ::pipcore::detail::builtin_bswap16
#endif
#ifndef __builtin_bswap32
#define __builtin_bswap32 ::pipcore::detail::builtin_bswap32
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#ifndef PIPCORE_ALWAYS_INLINE
#define PIPCORE_ALWAYS_INLINE __attribute__((always_inline))
#endif
#else
#ifndef PIPCORE_ALWAYS_INLINE
#define PIPCORE_ALWAYS_INLINE
#endif
#endif

#if PIPCORE_TARGET_ESP32
#include <esp_attr.h>
#endif

#if PIPCORE_TARGET_ESP32
#ifndef PIPCORE_HOT
#define PIPCORE_HOT IRAM_ATTR
#endif
#else
#ifndef PIPCORE_HOT
#define PIPCORE_HOT
#endif
#endif

#define PIPCORE_PP_CAT_IMPL(a, b) a##b
#define PIPCORE_PP_CAT(a, b) PIPCORE_PP_CAT_IMPL(a, b)

#define PIPCORE_DISPLAY_TAG_NONE 0
#define PIPCORE_DISPLAY_TAG_ST7789 1
#define PIPCORE_DISPLAY_TAG_ILI9488 2
#define PIPCORE_DISPLAY_TAG_SIMULATOR 3
#define PIPCORE_DISPLAY_TAG_ST7796 4

#define PIPCORE_DISPLAY_ID(name) PIPCORE_PP_CAT(PIPCORE_DISPLAY_TAG_, name)

#ifndef PIPCORE_DISPLAY
#if defined(CONFIG_PIPCORE_DISPLAY_ST7789)
#define PIPCORE_DISPLAY ST7789
#elif defined(CONFIG_PIPCORE_DISPLAY_ILI9488)
#define PIPCORE_DISPLAY ILI9488
#elif defined(CONFIG_PIPCORE_DISPLAY_ST7796)
#define PIPCORE_DISPLAY ST7796
#elif PIPCORE_TARGET_DESKTOP
#define PIPCORE_DISPLAY SIMULATOR
#else
#define PIPCORE_DISPLAY NONE
#endif
#endif

#ifndef PIPCORE_ENABLE_DEBUG
#ifdef CONFIG_PIPCORE_ENABLE_DEBUG
#define PIPCORE_ENABLE_DEBUG 1
#else
#define PIPCORE_ENABLE_DEBUG 0
#endif
#endif

#ifndef PIPCORE_DEBUG_CONSOLE
#ifdef CONFIG_PIPCORE_DEBUG_CONSOLE
#define PIPCORE_DEBUG_CONSOLE 1
#else
#define PIPCORE_DEBUG_CONSOLE 0
#endif
#endif

#ifndef PIPCORE_DEBUG_TRANSPORT_UART0
#ifdef CONFIG_PIPCORE_DEBUG_TRANSPORT_UART0
#define PIPCORE_DEBUG_TRANSPORT_UART0 1
#else
#define PIPCORE_DEBUG_TRANSPORT_UART0 0
#endif
#endif

#ifndef PIPCORE_ENABLE_PREFS
#if PIPCORE_TARGET_ESP32
#ifdef CONFIG_PIPCORE_ENABLE_PREFS
#define PIPCORE_ENABLE_PREFS 1
#else
#define PIPCORE_ENABLE_PREFS 0
#endif
#else
#define PIPCORE_ENABLE_PREFS 1
#endif
#endif

#ifndef PIPCORE_ENABLE_GRAPHICS
#if PIPCORE_TARGET_ESP32
#ifdef CONFIG_PIPCORE_ENABLE_GRAPHICS
#define PIPCORE_ENABLE_GRAPHICS 1
#else
#define PIPCORE_ENABLE_GRAPHICS 0
#endif
#else
#define PIPCORE_ENABLE_GRAPHICS 1
#endif
#endif

#ifndef PIPCORE_ENABLE_WIFI
#ifdef CONFIG_PIPCORE_ENABLE_WIFI
#define PIPCORE_ENABLE_WIFI 1
#else
#define PIPCORE_ENABLE_WIFI 0
#endif
#endif

#ifndef PIPCORE_ENABLE_OTA
#ifdef CONFIG_PIPCORE_ENABLE_OTA
#define PIPCORE_ENABLE_OTA 1
#else
#define PIPCORE_ENABLE_OTA 0
#endif
#endif

#ifndef PIPCORE_OTA_PROJECT_URL
#ifdef CONFIG_PIPCORE_OTA_PROJECT_URL
#define PIPCORE_OTA_PROJECT_URL CONFIG_PIPCORE_OTA_PROJECT_URL
#else
#define PIPCORE_OTA_PROJECT_URL ""
#endif
#endif

#ifndef PIPCORE_ENABLE_TOUCH
#ifdef CONFIG_PIPCORE_ENABLE_TOUCH
#define PIPCORE_ENABLE_TOUCH 1
#else
#define PIPCORE_ENABLE_TOUCH 0
#endif
#endif

#ifndef PIPCORE_ENABLE_AUDIO
#ifdef CONFIG_PIPCORE_ENABLE_AUDIO
#define PIPCORE_ENABLE_AUDIO 1
#else
#define PIPCORE_ENABLE_AUDIO 0
#endif
#endif

#ifndef PIPCORE_ENABLE_STORAGE
#if PIPCORE_TARGET_ESP32
#ifdef CONFIG_PIPCORE_ENABLE_STORAGE
#define PIPCORE_ENABLE_STORAGE 1
#else
#define PIPCORE_ENABLE_STORAGE 0
#endif
#else
#define PIPCORE_ENABLE_STORAGE 1
#endif
#endif

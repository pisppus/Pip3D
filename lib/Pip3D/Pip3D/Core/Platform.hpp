#pragma once

#include <cstdint>

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM) || defined(ESP32)
#define PIP3D_TARGET_ESP32 1
#else
#define PIP3D_TARGET_ESP32 0
#endif

#if defined(PIP3D_PC)
#define PIP3D_TARGET_PC 1
#else
#define PIP3D_TARGET_PC 0
#endif

#if PIP3D_TARGET_ESP32
#include <esp_attr.h>
#endif

#if !PIP3D_TARGET_PC
#include <Arduino.h>
#endif

#include "Debug/Logging.hpp"

#if defined(_MSC_VER)
#define PIP3D_FORCE_INLINE __forceinline
#define PIP3D_FLATTEN __forceinline
#define PIP3D_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define PIP3D_FORCE_INLINE inline __attribute__((always_inline))
#define PIP3D_FLATTEN __attribute__((flatten))
#define PIP3D_NOINLINE __attribute__((noinline))
#else
#define PIP3D_FORCE_INLINE inline
#define PIP3D_FLATTEN
#define PIP3D_NOINLINE
#endif

#if defined(_MSC_VER)
#define PIP3D_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define PIP3D_RESTRICT __restrict__
#else
#define PIP3D_RESTRICT
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PIP3D_HOT __attribute__((hot))
#define PIP3D_COLD __attribute__((cold))
#else
#define PIP3D_HOT
#define PIP3D_COLD
#endif

#if !defined(IRAM_ATTR)
#define IRAM_ATTR
#endif

#if PIP3D_TARGET_ESP32
#define PIP3D_FAST_DATA DRAM_ATTR
#else
#define PIP3D_FAST_DATA
#endif

#define PIP3D_CACHELINE_SIZE 32

#if defined(__GNUC__) || defined(__clang__)
#define PIP3D_PREFETCH_R(ptr) __builtin_prefetch((ptr), 0, 0)
#define PIP3D_PREFETCH_W(ptr) __builtin_prefetch((ptr), 1, 0)
#else
#define PIP3D_PREFETCH_R(ptr) ((void)0)
#define PIP3D_PREFETCH_W(ptr) ((void)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif
#else
#ifndef likely
#define likely(x) (x)
#endif
#ifndef unlikely
#define unlikely(x) (x)
#endif
#endif

#ifndef PIP3D_SCREEN_WIDTH
  #define PIP3D_SCREEN_WIDTH 320
#endif
#ifndef PIP3D_SCREEN_HEIGHT
  #define PIP3D_SCREEN_HEIGHT 240
#endif
#ifndef PIP3D_SCREEN_BAND_COUNT
  #define PIP3D_SCREEN_BAND_COUNT 2
#endif

#if PIP3D_TARGET_PC
#include <cstdlib>
inline long random(long minVal, long maxVal)
{
    if (maxVal <= minVal)
        return minVal;
    const unsigned long range = static_cast<unsigned long>(maxVal - minVal);
    const unsigned long r = static_cast<unsigned long>(std::rand());
    return minVal + static_cast<long>(r % range);
}
#endif

namespace pip3D
{
    template <typename T>
    PIP3D_FORCE_INLINE constexpr T clamp(T value, T min_val, T max_val)
    {
        return value < min_val ? min_val : (value > max_val ? max_val : value);
    }

    inline constexpr uint16_t SCREEN_WIDTH       = PIP3D_SCREEN_WIDTH;
    inline constexpr uint16_t SCREEN_HEIGHT      = PIP3D_SCREEN_HEIGHT;
    inline constexpr uint16_t SCREEN_BAND_COUNT  = PIP3D_SCREEN_BAND_COUNT;
    inline constexpr uint16_t SCREEN_BAND_HEIGHT = SCREEN_HEIGHT / SCREEN_BAND_COUNT;

    inline int16_t  g_bandOffsetY = 0;
    inline int16_t  g_bandHeight  = static_cast<int16_t>(SCREEN_HEIGHT);
    inline uint32_t g_frameStamp  = 0;

    struct Display
    {
        uint16_t width     = PIP3D_SCREEN_WIDTH;
        uint16_t height    = PIP3D_SCREEN_HEIGHT;
        int8_t   cs        = 10;
        int8_t   dc        = 9;
        int8_t   rst       = 8;
        int8_t   bl        = -1;
        int8_t   mosi      = 11;
        int8_t   sclk      = 12;
        uint8_t  rotation  = 1;
        uint32_t spi_freq  = 80000000;

        Display() = default;
        Display(uint16_t w, uint16_t h) : width(w), height(h) {}
        Display(uint16_t w, uint16_t h, int8_t cs_, int8_t dc_, int8_t rst_)
            : width(w), height(h), cs(cs_), dc(dc_), rst(rst_) {}
    };
    using DisplayConfig = Display;

    struct Viewport
    {
        int16_t  x      = 0;
        int16_t  y      = 0;
        uint16_t width  = SCREEN_WIDTH;
        uint16_t height = SCREEN_HEIGHT;

        Viewport() = default;
        Viewport(int16_t x_, int16_t y_, uint16_t w_, uint16_t h_)
            : x(x_), y(y_), width(w_), height(h_) {}
        Viewport(uint16_t w_, uint16_t h_) : width(w_), height(h_) {}
    };
}
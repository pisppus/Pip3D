#pragma once

#if defined(PIP3D_PC)
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <limits>
#if defined(_MSC_VER)
#include <malloc.h>
#endif
#else
#include <Arduino.h>
#include <SPI.h>
#include <array>
#include <cstdlib>
#include <cstring>
#endif

#include "Debug/Logging.hpp"

#if defined(PIP3D_PC)
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
    __attribute__((always_inline)) inline constexpr T clamp(T value, T min_val, T max_val)
    {
        return value < min_val ? min_val : (value > max_val ? max_val : value);
    }

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

    struct CoreConfig
    {
        static constexpr uint32_t CORE_FREQ = 240000000;
        static constexpr uint32_t SPI_FREQ = 80000000;
        static constexpr uint16_t DEFAULT_WIDTH = 320;
        static constexpr uint16_t DEFAULT_HEIGHT = 240;
        static constexpr float DEFAULT_FOV = 60.0f;
        static constexpr float EPSILON = 1e-6f;

        struct Performance
        {
            static constexpr int FPS_SAMPLES = 60;
            static constexpr int MAX_FPS = 120;
            static constexpr int MIN_FPS = 5;
            static constexpr uint32_t FRAME_TIME_US = 16667;
        };

        struct Rendering
        {
            static constexpr int MAX_VERTICES = 10000;
            static constexpr int MAX_TRIANGLES = 20000;
            static constexpr float Z_NEAR = 0.1f;
            static constexpr float Z_FAR = 1000.0f;
            static constexpr uint16_t BAND_HEIGHT = 40;
        };
    };
}
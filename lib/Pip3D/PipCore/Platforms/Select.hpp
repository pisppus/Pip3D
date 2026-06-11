#pragma once

#include <PipCore/Config/Features.hpp>
#include <PipCore/Platform.hpp>

#ifndef PIPCORE_PLATFORM
#error "Platform not selected. Define PIPCORE_PLATFORM in config.hpp"
#endif

#if PIPCORE_TARGET_ESP32 && (PIPCORE_PLATFORM_ID(PIPCORE_PLATFORM) == PIPCORE_PLATFORM_TAG_ESP32)
#include <PipCore/Platforms/ESP32/Platform.hpp>
#elif PIPCORE_TARGET_DESKTOP && (PIPCORE_PLATFORM_ID(PIPCORE_PLATFORM) == PIPCORE_PLATFORM_TAG_DESKTOP)
#include <PipCore/Platforms/Desktop/Platform.hpp>
#else
#error "Unsupported PIPCORE_PLATFORM value for this target"
#endif

namespace pipcore
{
#if PIPCORE_TARGET_ESP32 && (PIPCORE_PLATFORM == PIPCORE_PLATFORM_TAG_ESP32)
    using SelectedPlatform = esp32::Platform;
#elif PIPCORE_TARGET_DESKTOP && (PIPCORE_PLATFORM == PIPCORE_PLATFORM_TAG_DESKTOP)
    using SelectedPlatform = desktop::Platform;
#endif

    [[nodiscard]] inline Platform *GetPlatform() noexcept
    {
        static SelectedPlatform instance;
        return &instance;
    }
}

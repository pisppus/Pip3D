#pragma once

#include <PipCore/Config/Features.hpp>

#ifndef PIPCORE_DISPLAY
#error "Display not selected. Define PIPCORE_DISPLAY in config.hpp"
#endif

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789
#include <PipCore/Displays/ST7789/Display.hpp>
#elif PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ILI9488
#include <PipCore/Displays/ILI9488/Display.hpp>
#elif PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_SIMULATOR
#include <PipCore/Displays/Simulator/Display.hpp>
#endif

namespace pipcore
{
#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789
    using SelectedDisplay = st7789::Display;
#elif PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ILI9488
    using SelectedDisplay = ili9488::Display;
#elif PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_SIMULATOR
    using SelectedDisplay = simulator::Display;
#else
#error "Unsupported PIPCORE_DISPLAY value"
#endif
}

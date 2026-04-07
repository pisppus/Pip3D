#pragma once

#if __has_include(<config.hpp>)
#include <config.hpp>
#endif

#define PIPCORE_PP_CAT_IMPL(a, b) a##b
#define PIPCORE_PP_CAT(a, b) PIPCORE_PP_CAT_IMPL(a, b)
#define PIPCORE_DISPLAY_TAG_ST7789 1
#define PIPCORE_DISPLAY_TAG_ILI9488 2
#define PIPCORE_DISPLAY_ID(name) PIPCORE_PP_CAT(PIPCORE_DISPLAY_TAG_, name)

#ifndef PIPCORE_DISPLAY
#define PIPCORE_DISPLAY ST7789
#endif

#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789
#include <pipCore/Platforms/ESP32/Transports/St7789Spi.hpp>
#elif PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ILI9488
#include <pipCore/Platforms/ESP32/Transports/Ili9488Spi.hpp>
#else
#error "Unsupported display transport for selected PIPCORE_DISPLAY"
#endif

namespace pipcore::esp32
{
#if PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ST7789
    using SelectedDisplayTransport = St7789Spi;
#elif PIPCORE_DISPLAY_ID(PIPCORE_DISPLAY) == PIPCORE_DISPLAY_TAG_ILI9488
    using SelectedDisplayTransport = Ili9488Spi;
#endif
}

#undef PIPCORE_DISPLAY_ID
#undef PIPCORE_DISPLAY_TAG_ST7789
#undef PIPCORE_DISPLAY_TAG_ILI9488
#undef PIPCORE_PP_CAT
#undef PIPCORE_PP_CAT_IMPL

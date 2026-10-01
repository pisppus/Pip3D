#include "Debug/Logging.hpp"
#include "Core/Platform.hpp"

#if PIP3D_ENABLE_LOGGING

#include <cstdio>
#include <cstdarg>

#if PIP3D_TARGET_ESP32
#include <esp_timer.h>
#define GET_MICROS() static_cast<uint32_t>(esp_timer_get_time())
#else
#include <chrono>
static inline uint32_t getHostMicros()
{
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}
#define GET_MICROS() getHostMicros()
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace pip3D
{
    namespace Debug
    {
        namespace
        {
            struct LoggerState
            {
                LogLevel level;
                uint16_t modules;
                bool timestamps;
                bool initialized;
                uint32_t startMicros;
                LogLevel moduleLevels[8];
            };

            LoggerState g_state = {
                static_cast<LogLevel>(PIP3D_LOG_DEFAULT_LEVEL),
                LOG_MODULE_ALL,
                true,
                false,
                0u,
                {LOG_LEVEL_OFF, LOG_LEVEL_OFF, LOG_LEVEL_OFF, LOG_LEVEL_OFF,
                 LOG_LEVEL_OFF, LOG_LEVEL_OFF, LOG_LEVEL_OFF, LOG_LEVEL_OFF}};

            constexpr const char *kLevelNames[6] = {
                "OFF", "ERR", "WRN", "INF", "DBG", "TRC"};
            constexpr const char *kModuleNames[8] = {
                "COR", "RDR", "PHY", "CAM", "SCN", "RES", "PRF", "USR"};

            static_assert(LOG_MODULE_USER == (1u << 7),
                          "moduleLevels[] holds 8 entries; USER must be bit 7.");

            PIP3D_FORCE_INLINE int moduleIndex(uint16_t module)
            {
                if (module == 0u)
                    return 0;
#if defined(_MSC_VER)
                unsigned long idx = 0;
                if (_BitScanForward(&idx, static_cast<unsigned long>(module)))
                    return (idx < 8) ? static_cast<int>(idx) : 0;
                return 0;
#else
                int idx = __builtin_ctz(static_cast<unsigned int>(module));
                return (idx < 8) ? idx : 0;
#endif
            }
        }

        void Logger::init(LogLevel level, uint16_t modulesMask, bool timestamps)
        {
            g_state.level = level;
            g_state.modules = modulesMask;
            g_state.timestamps = timestamps;
            if (!g_state.initialized)
            {
                g_state.startMicros = GET_MICROS();
                g_state.initialized = true;
            }
        }

        void Logger::setLevel(LogLevel level) { g_state.level = level; }
        LogLevel Logger::getLevel() { return g_state.level; }

        void Logger::setModules(uint16_t mask) { g_state.modules = mask; }
        void Logger::enableModule(uint16_t module) { g_state.modules |= module; }
        void Logger::disableModule(uint16_t module) { g_state.modules &= static_cast<uint16_t>(~module); }
        uint16_t Logger::getModules() { return g_state.modules; }

        void Logger::setModuleLevel(uint16_t module, LogLevel level)
        {
            g_state.moduleLevels[moduleIndex(module)] = level;
        }

        LogLevel Logger::getModuleLevel(uint16_t module)
        {
            return g_state.moduleLevels[moduleIndex(module)];
        }

        void Logger::clearModuleLevels()
        {
            for (int i = 0; i < 8; ++i)
                g_state.moduleLevels[i] = LOG_LEVEL_OFF;
        }

        void Logger::setProfileSilent()
        {
            clearModuleLevels();
            g_state.level = LOG_LEVEL_OFF;
            g_state.modules = LOG_MODULE_ALL;
        }

        void Logger::setProfilePerformance()
        {
            clearModuleLevels();
            g_state.level = LOG_LEVEL_WARNING;
            g_state.modules = LOG_MODULE_ALL;
            g_state.moduleLevels[moduleIndex(LOG_MODULE_PERFORMANCE)] = LOG_LEVEL_INFO;
            g_state.moduleLevels[moduleIndex(LOG_MODULE_CORE)] = LOG_LEVEL_INFO;
        }

        void Logger::setProfileVerboseAll()
        {
            clearModuleLevels();
            g_state.level = LOG_LEVEL_TRACE;
            g_state.modules = LOG_MODULE_ALL;
        }

        void Logger::setTimestampsEnabled(bool enabled) { g_state.timestamps = enabled; }
        bool Logger::getTimestampsEnabled() { return g_state.timestamps; }

        bool Logger::isEnabled(uint16_t module, LogLevel level)
        {
            if (level == LOG_LEVEL_OFF)
                return false;
            if ((g_state.modules & module) == 0u)
                return false;
            const LogLevel perModule = g_state.moduleLevels[moduleIndex(module)];
            const LogLevel effective = (perModule != LOG_LEVEL_OFF) ? perModule : g_state.level;
            return level <= effective;
        }

        void Logger::log(uint16_t module, LogLevel level, const char *fmt, ...)
        {
            if (!isEnabled(module, level))
                return;

            if (!g_state.initialized)
            {
                g_state.startMicros = GET_MICROS();
                g_state.initialized = true;
            }

            char line[192];
            int prefixLen = 0;

            const char *lvlStr = kLevelNames[level];
            const char *modStr = kModuleNames[moduleIndex(module)];

            if (g_state.timestamps)
            {
                const uint32_t now = GET_MICROS();
                const uint32_t dt = now - g_state.startMicros;
                const uint32_t ms = dt / 1000u;
                const uint32_t s = ms / 1000u;
                const uint16_t msRem = static_cast<uint16_t>(ms % 1000u);
                prefixLen = snprintf(line, sizeof(line),
                                     "[%lu.%03u] %s %s: ",
                                     static_cast<unsigned long>(s),
                                     static_cast<unsigned int>(msRem),
                                     lvlStr, modStr);
            }
            else
            {
                prefixLen = snprintf(line, sizeof(line), "%s %s: ", lvlStr, modStr);
            }

            if (prefixLen <= 0 || prefixLen >= static_cast<int>(sizeof(line)))
                return;

            va_list args;
            va_start(args, fmt);
            vsnprintf(line + prefixLen, sizeof(line) - static_cast<size_t>(prefixLen), fmt, args);
            va_end(args);

            std::printf("%s\n", line);
        }
    }
}

#endif
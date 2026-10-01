#pragma once

#include <stdint.h>
#include "Debug/Flags.hpp"

namespace pip3D
{
    namespace Debug
    {
        enum LogLevel : uint8_t
        {
            LOG_LEVEL_OFF = 0,
            LOG_LEVEL_ERROR = 1,
            LOG_LEVEL_WARNING = 2,
            LOG_LEVEL_INFO = 3,
            LOG_LEVEL_DEBUG = 4,
            LOG_LEVEL_TRACE = 5
        };

        enum LogModule : uint16_t
        {
            LOG_MODULE_CORE = 1u << 0,
            LOG_MODULE_RENDER = 1u << 1,
            LOG_MODULE_PHYSICS = 1u << 2,
            LOG_MODULE_CAMERA = 1u << 3,
            LOG_MODULE_SCENE = 1u << 4,
            LOG_MODULE_RESOURCES = 1u << 5,
            LOG_MODULE_PERFORMANCE = 1u << 6,
            LOG_MODULE_USER = 1u << 7,
            LOG_MODULE_ALL = 0xFFFFu
        };

        class Logger
        {
        public:
            static void init(LogLevel level = LOG_LEVEL_INFO,
                             uint16_t modulesMask = LOG_MODULE_ALL,
                             bool timestamps = true);
            static void setLevel(LogLevel level);
            static LogLevel getLevel();

            static void setModules(uint16_t mask);
            static void enableModule(uint16_t module);
            static void disableModule(uint16_t module);
            static uint16_t getModules();

            static void setModuleLevel(uint16_t module, LogLevel level);
            static LogLevel getModuleLevel(uint16_t module);
            static void clearModuleLevels();

            static void setProfileSilent();
            static void setProfilePerformance();
            static void setProfileVerboseAll();

            static void setTimestampsEnabled(bool enabled);
            static bool getTimestampsEnabled();

            static bool isEnabled(uint16_t module, LogLevel level);
            static void log(uint16_t module, LogLevel level, const char *fmt, ...);
        };
    }
}

#if PIP3D_ENABLE_LOGGING

#define LOG(module, level, fmt, ...) ::pip3D::Debug::Logger::log(module, level, fmt, ##__VA_ARGS__)
#define LOGE(module, fmt, ...) LOG(module, ::pip3D::Debug::LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOGW(module, fmt, ...) LOG(module, ::pip3D::Debug::LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define LOGI(module, fmt, ...) LOG(module, ::pip3D::Debug::LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOGD(module, fmt, ...) LOG(module, ::pip3D::Debug::LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOGT(module, fmt, ...) LOG(module, ::pip3D::Debug::LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)

#define LOG_SET_ERROR() ::pip3D::Debug::Logger::setLevel(::pip3D::Debug::LOG_LEVEL_ERROR)
#define LOG_SET_WARN() ::pip3D::Debug::Logger::setLevel(::pip3D::Debug::LOG_LEVEL_WARNING)
#define LOG_SET_INFO() ::pip3D::Debug::Logger::setLevel(::pip3D::Debug::LOG_LEVEL_INFO)
#define LOG_SET_DEBUG() ::pip3D::Debug::Logger::setLevel(::pip3D::Debug::LOG_LEVEL_DEBUG)
#define LOG_SET_TRACE() ::pip3D::Debug::Logger::setLevel(::pip3D::Debug::LOG_LEVEL_TRACE)

#define LOG_PROFILE_SILENT() ::pip3D::Debug::Logger::setProfileSilent()
#define LOG_PROFILE_PERF() ::pip3D::Debug::Logger::setProfilePerformance()
#define LOG_PROFILE_VERBOSE() ::pip3D::Debug::Logger::setProfileVerboseAll()

#else

#define LOG(module, level, fmt, ...) ((void)0)
#define LOGE(module, fmt, ...) ((void)0)
#define LOGW(module, fmt, ...) ((void)0)
#define LOGI(module, fmt, ...) ((void)0)
#define LOGD(module, fmt, ...) ((void)0)
#define LOGT(module, fmt, ...) ((void)0)

#define LOG_SET_ERROR() ((void)0)
#define LOG_SET_WARN() ((void)0)
#define LOG_SET_INFO() ((void)0)
#define LOG_SET_DEBUG() ((void)0)
#define LOG_SET_TRACE() ((void)0)

#define LOG_PROFILE_SILENT() ((void)0)
#define LOG_PROFILE_PERF() ((void)0)
#define LOG_PROFILE_VERBOSE() ((void)0)

#endif
#pragma once

#include "Core/Platform.hpp"

#if PIP3D_TARGET_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#endif

namespace pip3D
{
    using JobFunc = void (*)(void *userData);

    struct Job
    {
        JobFunc func;
        void *userData;
    };

    class JobSystem
    {
    public:
        static bool init();
        static void shutdown();
        static bool submit(JobFunc func, void *userData = nullptr);
        static bool waitAll();
        PIP3D_FORCE_INLINE static bool isEnabled() { return s_running; }

    private:
        static void workerLoop(void *param);
        static volatile bool s_running;
    };
}
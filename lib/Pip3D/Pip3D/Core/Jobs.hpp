#pragma once

#include "Core/Platform.hpp"

#ifdef ARDUINO_ARCH_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#endif

namespace pip3D
{
    typedef void (*JobFunc)(void *userData);

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
        static bool waitAll(uint32_t timeoutTicks = portMAX_DELAY);
        static int pendingCount();
        static bool isEnabled();

    private:
        static void workerLoop(void *param);
    };

    void useDualCore(bool enabled);
    bool isDualCoreEnabled();
}

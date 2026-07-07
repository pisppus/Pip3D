#include "Core/Jobs.hpp"
#include "Debug/Logging.hpp"

namespace pip3D
{

#if PIP3D_TARGET_ESP32

    static constexpr int MAX_JOBS = 32;
    static constexpr int JOB_MASK = MAX_JOBS - 1;
    static constexpr uint32_t STACK_SIZE = 4096;
    static constexpr UBaseType_t WORKER_PRIORITY = 1;
    static constexpr BaseType_t WORKER_CORE = 0;

    static Job s_jobQueue[MAX_JOBS];
    static volatile int s_head = 0;
    static volatile int s_tail = 0;

    static TaskHandle_t s_workerTask = nullptr;
    static SemaphoreHandle_t s_doneSemaphore = nullptr;

    volatile bool JobSystem::s_running = false;

    PIP3D_FORCE_INLINE static void memFence() { __sync_synchronize(); }

    bool JobSystem::init()
    {
#if CONFIG_FREERTOS_UNICORE
        s_running = false;
        LOGW(::pip3D::Debug::LOG_MODULE_CORE,
             "JobSystem disabled: FreeRTOS unicore configuration");
        return false;
#else
        if (s_running)
            return true;

        s_doneSemaphore = xSemaphoreCreateCounting(MAX_JOBS, 0);
        if (!s_doneSemaphore)
        {
            LOGE(::pip3D::Debug::LOG_MODULE_CORE,
                 "JobSystem::init: done counting semaphore failed");
            return false;
        }

        s_head = 0;
        s_tail = 0;

        const BaseType_t res = xTaskCreatePinnedToCore(
            JobSystem::workerLoop,
            "Pip3DJobWorker",
            STACK_SIZE,
            nullptr,
            WORKER_PRIORITY,
            &s_workerTask,
            WORKER_CORE);

        if (res != pdPASS)
        {
            vSemaphoreDelete(s_doneSemaphore);
            s_doneSemaphore = nullptr;
            s_workerTask = nullptr;
            LOGE(::pip3D::Debug::LOG_MODULE_CORE,
                 "JobSystem::init: xTaskCreatePinnedToCore returned %d",
                 (int)res);
            return false;
        }

        s_running = true;
        return true;
#endif
    }

    void JobSystem::shutdown()
    {
        if (!s_running)
            return;

        waitAll();

        s_running = false;

        if (s_workerTask)
        {
            xTaskNotifyGive(s_workerTask);
            vTaskDelay(1);
            vTaskDelete(s_workerTask);
            s_workerTask = nullptr;
        }

        if (s_doneSemaphore)
        {
            vSemaphoreDelete(s_doneSemaphore);
            s_doneSemaphore = nullptr;
        }

        s_head = 0;
        s_tail = 0;
    }

    bool JobSystem::submit(JobFunc func, void *userData)
    {
        if (unlikely(!s_running || !func))
        {
            LOGW(::pip3D::Debug::LOG_MODULE_CORE,
                 "JobSystem::submit rejected: running=%d func=%p",
                 (int)s_running, (void *)func);
            return false;
        }

        const int head = s_head;
        const int nextHead = (head + 1) & JOB_MASK;

        if (nextHead == s_tail)
        {
            LOGW(::pip3D::Debug::LOG_MODULE_CORE,
                 "JobSystem::submit: queue full, job rejected");
            return false;
        }

        s_jobQueue[head].func = func;
        s_jobQueue[head].userData = userData;
        memFence();
        s_head = nextHead;

        if (s_workerTask)
            xTaskNotifyGive(s_workerTask);

        return true;
    }

    bool JobSystem::waitAll()
    {
        if (!s_running)
            return true;

        for (;;)
        {
            while (xSemaphoreTake(s_doneSemaphore, 0) == pdTRUE)
            {
            }

            memFence();
            const int pending = (s_head - s_tail + MAX_JOBS) & JOB_MASK;

            if (pending == 0)
                return true;

            if (xSemaphoreTake(s_doneSemaphore, portMAX_DELAY) != pdTRUE)
                return false;
        }
    }

    void JobSystem::workerLoop(void *param)
    {
        (void)param;

        for (;;)
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            if (!s_running)
                return;

            for (;;)
            {
                const int tail = s_tail;
                if (tail == s_head)
                    break;

                memFence();
                Job job = s_jobQueue[tail];
                s_tail = (tail + 1) & JOB_MASK;

                if (job.func)
                    job.func(job.userData);

                xSemaphoreGive(s_doneSemaphore);
            }
        }
    }

#else

    volatile bool JobSystem::s_running = false;

    bool JobSystem::init()
    {
        s_running = false;
        return false;
    }

    void JobSystem::shutdown() { s_running = false; }

    bool JobSystem::submit(JobFunc func, void *userData)
    {
        if (!func)
            return false;
        func(userData);
        return true;
    }

    bool JobSystem::waitAll() { return true; }

    void JobSystem::workerLoop(void *param) { (void)param; }

#endif
}
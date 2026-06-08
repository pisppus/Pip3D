#pragma once

#include "Core/Platform.h"
#include <stdint.h>
#include <array>
#include <cstring>

#include <Arduino.h>

namespace pip3D
{
    inline float getDeltaTime()
    {
        static uint32_t lastMicros = 0;
        uint32_t now = micros();
        if (lastMicros == 0)
        {
            lastMicros = now;
            return 0.0f;
        }
        uint32_t diff = now - lastMicros;
        lastMicros = now;
        float dt = diff * 1e-6f;
        if (dt < 0.0f)
        {
            dt = 0.0f;
        }
        if (dt > 0.1f)
        {
            dt = 0.1f;
        }
        return dt;
    }

    class alignas(16) PerfCounter
    {
    private:
        uint32_t frameCount = 0, lastTime = 0, frameTime = 0;
        float currentFPS = 0, avgFPS = 0, sum = 0;

        static constexpr int SAMPLES = 60;
        std::array<float, SAMPLES> history{};
        int idx = 0;
        bool firstReceived = false;
        uint32_t fpsLastTime = 0;
        bool fpsInitialized = false;

    public:
        PerfCounter() = default;

        void begin() { lastTime = micros(); }

        void endFrame()
        {
            const uint32_t now = micros();
            frameTime = now - lastTime;
            frameCount++;

            if (frameTime > 0u)
            {
                const float fps = 1000000.0f / static_cast<float>(frameTime);
                currentFPS = fps;
            }

            if (!fpsInitialized)
            {
                fpsLastTime = now;
                fpsInitialized = true;
            }

            const uint32_t dt = now - fpsLastTime;
            if (dt >= 1000000u)
            {
                const float accurateFPS = frameCount * 1000000.0f / static_cast<float>(dt);
                frameCount = 0;
                fpsLastTime = now;

                if (!firstReceived)
                {
                    for (int i = 0; i < SAMPLES; ++i)
                        history[i] = accurateFPS;
                    sum = accurateFPS * static_cast<float>(SAMPLES);
                    avgFPS = accurateFPS;
                    firstReceived = true;
                }
                else
                {
                    sum -= history[idx];
                    history[idx] = accurateFPS;
                    sum += accurateFPS;
                    idx = (idx + 1) % SAMPLES;
                    avgFPS = sum / static_cast<float>(SAMPLES);
                }
            }
            else if (!firstReceived)
            {
                avgFPS = currentFPS;
            }

            lastTime = now;
        }

        float getFPS() const { return currentFPS; }
        float getAvgFPS() const { return avgFPS; }
        float getAverageFPS() const { return avgFPS; }
        uint32_t getFrameTime() const { return frameTime; }
        uint32_t getFrameTimeMs() const { return frameTime / 1000; }

        bool isStable() const { return firstReceived && (avgFPS > 5); }
        float getEfficiency() const { return clamp(avgFPS / 120.0f, 0.0f, 1.0f); }

        void reset()
        {
            frameCount = 0;
            currentFPS = 0.0f;
            avgFPS = 0.0f;
            firstReceived = false;
            sum = 0.0f;
            idx = 0;
            frameTime = 0;
            fpsLastTime = 0;
            fpsInitialized = false;
        }
    };

    using PerformanceCounter = PerfCounter;
    struct Profiler
    {
    private:
        struct Section
        {
            const char *name;
            uint32_t startTime;
            uint32_t totalTime;
            uint32_t callCount;
            bool active;
        };

        static constexpr int MAX_SECTIONS = 16;
        static Section sections[MAX_SECTIONS];
        static int sectionCount;
        static int currentSection;

    public:
        static void beginSection(const char *name)
        {
            if (!name)
                return;
            int idx = -1;

            for (int i = 0; i < sectionCount; i++)
            {
                if (strcmp(sections[i].name, name) == 0)
                {
                    idx = i;
                    break;
                }
            }

            if (idx == -1 && sectionCount < MAX_SECTIONS)
            {
                idx = sectionCount++;
                sections[idx] = {name, 0, 0, 0, false};
            }

            if (idx != -1)
            {
                sections[idx].startTime = micros();
                sections[idx].active = true;
                currentSection = idx;
            }
        }

        static void endSection()
        {
            if (currentSection >= 0 && currentSection < sectionCount)
            {
                Section &sec = sections[currentSection];
                if (sec.active)
                {
                    uint32_t elapsed = micros() - sec.startTime;
                    sec.totalTime += elapsed;
                    sec.callCount++;
                    sec.active = false;
                }
            }
            currentSection = -1;
        }

        static void printReport()
        {
            uint32_t totalFrameTime = 0;

            for (int i = 0; i < sectionCount; i++)
            {
                totalFrameTime += sections[i].totalTime;
            }

            for (int i = 0; i < sectionCount; i++)
            {
                Section &sec = sections[i];
                if (sec.callCount > 0)
                {
                    float avgTime = sec.totalTime / (float)sec.callCount;
                    float percentage = totalFrameTime > 0 ? (sec.totalTime * 100.0f / totalFrameTime) : 0;

                    LOGI(::pip3D::Debug::LOG_MODULE_PERFORMANCE,
                         "%s: %.2fms avg, %d calls, %.1f%%",
                         sec.name,
                         avgTime / 1000.0f,
                         sec.callCount,
                         percentage);
                }
            }

            LOGI(::pip3D::Debug::LOG_MODULE_PERFORMANCE,
                 "Total: %.2fms", totalFrameTime / 1000.0f);
        }

        static void reset()
        {
            for (int i = 0; i < sectionCount; i++)
            {
                sections[i].totalTime = 0;
                sections[i].callCount = 0;
                sections[i].active = false;
            }
            currentSection = -1;
        }

        static float getSectionTime(const char *name)
        {
            for (int i = 0; i < sectionCount; i++)
            {
                if (strcmp(sections[i].name, name) == 0 && sections[i].callCount > 0)
                {
                    return sections[i].totalTime / (float)sections[i].callCount / 1000.0f;
                }
            }
            return 0.0f;
        }
    };
}
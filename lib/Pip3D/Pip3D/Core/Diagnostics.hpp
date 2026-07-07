#pragma once

#include <cstdint>

#include <PipCore/Platform.hpp>
#include <PipCore/Platforms/Select.hpp>
#include "Core/Platform.hpp"

namespace pip3D
{
    PIP3D_FORCE_INLINE uint64_t getSystemMicros()
    {
        return pipcore::GetPlatform()->nowUs();
    }

    class PerfCounter
    {
    private:
        uint32_t frameCount = 0;
        uint32_t lastTime = 0;
        uint32_t frameTime = 0;
        uint32_t fpsLastTime = 0;
        float currentFPS = 0.0f;
        float avgFPS = 0.0f;
        float sum = 0.0f;

        static constexpr int SAMPLES = 60;
        float history[SAMPLES]{};
        int idx = 0;
        bool firstReceived = false;
        bool fpsInitialized = false;

    public:
        PerfCounter() = default;

        PIP3D_FORCE_INLINE void begin() { lastTime = static_cast<uint32_t>(getSystemMicros()); }

        void endFrame()
        {
            const uint32_t now = static_cast<uint32_t>(getSystemMicros());
            frameTime = now - lastTime;
            frameCount++;

            if (frameTime > 0u)
            {
                currentFPS = 1000000.0f / static_cast<float>(frameTime);
            }

            if (!fpsInitialized)
            {
                fpsLastTime = now;
                fpsInitialized = true;
            }

            const uint32_t dt = now - fpsLastTime;
            if (dt >= 1000000u)
            {
                const float accurateFPS =
                    static_cast<float>(frameCount) * (1000000.0f / static_cast<float>(dt));

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

                frameCount = 0;
                fpsLastTime = now;
            }
            else if (!firstReceived)
            {
                avgFPS = currentFPS;
            }

            lastTime = now;
        }

        PIP3D_FORCE_INLINE float getFPS() const { return currentFPS; }
        PIP3D_FORCE_INLINE float getAverageFPS() const { return avgFPS; }
        PIP3D_FORCE_INLINE uint32_t getFrameTime() const { return frameTime; }
    };
}
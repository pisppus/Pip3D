#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <atomic>

#if defined(ESP_PLATFORM) || defined(ESP32)
#include "esp_idf_version.h"
#include <esp_cpu.h>
#else
#include <chrono>
#endif

#ifndef PIPCORE_PP_CAT_IMPL
#define PIPCORE_PP_CAT_IMPL(a, b) a##b
#define PIPCORE_PP_CAT(a, b) PIPCORE_PP_CAT_IMPL(a, b)
#endif

namespace pipcore::debug
{

    struct ProfileNode
    {
        const char *name = nullptr;
        ProfileNode *next = nullptr;
        ProfileNode *parent = nullptr;
        uint64_t totalCycles = 0;
        uint64_t selfCycles = 0;
        uint32_t callCount = 0;
        uint32_t maxCycles = 0;
        bool registered = false;
    };

    class Profiler
    {
    public:
        ProfileNode *_head = nullptr;
        ProfileNode *_current = nullptr;
        std::atomic<bool> _lock{false};

        static Profiler &instance() noexcept
        {
            static Profiler prof;
            return prof;
        }

        inline void lock() noexcept
        {
            while (_lock.exchange(true, std::memory_order_acquire))
            {
            }
        }

        inline void unlock() noexcept
        {
            _lock.store(false, std::memory_order_release);
        }

        inline uint64_t getCycles() noexcept
        {
#if defined(ESP_PLATFORM) || defined(ESP32)
#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            return esp_cpu_get_cycle_count();
#else
            return esp_cpu_get_ccount();
#endif
#else
            auto now = std::chrono::high_resolution_clock::now();
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
#endif
        }

        void enterNode(ProfileNode *node, const char *name) noexcept
        {
            lock();
            if (!node->registered)
            {
                node->name = name;
                node->next = _head;
                _head = node;
                node->registered = true;
            }
            node->parent = _current;
            node->callCount++;
            _current = node;
            unlock();
        }

        void exitNode(ProfileNode *node, uint64_t startCycles) noexcept
        {
            const uint64_t endCycles = getCycles();
            const uint64_t elapsed = (endCycles >= startCycles) ? (endCycles - startCycles) : (0xFFFFFFFFu - startCycles + endCycles);

            lock();
            node->totalCycles += elapsed;
            if (elapsed > node->maxCycles)
            {
                node->maxCycles = static_cast<uint32_t>(elapsed);
            }
            _current = node->parent;
            unlock();
        }

        void calculateSelfCycles() noexcept
        {
            lock();
            for (ProfileNode *curr = _head; curr != nullptr; curr = curr->next)
            {
                uint64_t childCycles = 0;
                for (ProfileNode *child = _head; child != nullptr; child = child->next)
                {
                    if (child->parent == curr)
                    {
                        childCycles += child->totalCycles;
                    }
                }
                curr->selfCycles = (curr->totalCycles >= childCycles) ? (curr->totalCycles - childCycles) : 0;
            }
            unlock();
        }

        void clear() noexcept
        {
            lock();
            for (ProfileNode *curr = _head; curr != nullptr; curr = curr->next)
            {
                curr->totalCycles = 0;
                curr->selfCycles = 0;
                curr->callCount = 0;
                curr->maxCycles = 0;
            }
            _current = nullptr;
            unlock();
        }
    };

    class ScopeZone
    {
    public:
        ScopeZone(ProfileNode *node, const char *name) noexcept : _node(node)
        {
            _startCycles = Profiler::instance().getCycles();
            Profiler::instance().enterNode(node, name);
        }
        ~ScopeZone() noexcept
        {
            Profiler::instance().exitNode(_node, _startCycles);
        }

    private:
        ProfileNode *_node;
        uint64_t _startCycles;
    };

}

#define PIP_PROFILE_ZONE(name)                                            \
    static ::pipcore::debug::ProfileNode PIPCORE_PP_CAT(node_, __LINE__); \
    ::pipcore::debug::ScopeZone PIPCORE_PP_CAT(prof_, __LINE__)(&PIPCORE_PP_CAT(node_, __LINE__), name)

#define PIP_PROFILE_FUNCTION() PIP_PROFILE_ZONE(__PRETTY_FUNCTION__)
#pragma once

#include <cstddef>
#include <cstdint>

namespace pipcore::debug
{
    enum class MemoryEvent : uint8_t
    {
        Alloc,
        AllocFail,
        Free,
        Realloc,
        ReallocFail,
        HeapSample
    };

    using MemoryEventHandler = void (*)(MemoryEvent event,
                                        const char *tag,
                                        void *ptr,
                                        void *oldPtr,
                                        size_t bytes,
                                        uint32_t caps) noexcept;

    extern MemoryEventHandler g_memoryEventHandler;

    void setMemoryEventHandler(MemoryEventHandler handler) noexcept;
    [[nodiscard]] MemoryEventHandler memoryEventHandler() noexcept;

    inline void memoryEvent(MemoryEvent event,
                            const char *tag,
                            void *ptr,
                            void *oldPtr,
                            size_t bytes,
                            uint32_t caps) noexcept
    {
        if (g_memoryEventHandler != nullptr)
        {
            g_memoryEventHandler(event, tag, ptr, oldPtr, bytes, caps);
        }
    }
}
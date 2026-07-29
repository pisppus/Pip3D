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

    using MemoryHandler = void (*)(MemoryEvent event,
                                   const char *tag,
                                   void *ptr,
                                   void *oldPtr,
                                   size_t bytes,
                                   uint32_t caps) noexcept;

    extern MemoryHandler g_memoryHandler;

    void setMemoryHandler(MemoryHandler handler) noexcept;
    [[nodiscard]] MemoryHandler memoryHandler() noexcept;

    inline void memoryEvent(MemoryEvent event,
                            const char *tag,
                            void *ptr,
                            void *oldPtr,
                            size_t bytes,
                            uint32_t caps) noexcept
    {
        if (g_memoryHandler != nullptr)
        {
            g_memoryHandler(event, tag, ptr, oldPtr, bytes, caps);
        }
    }
}
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

    void setMemoryEventHandler(MemoryEventHandler handler) noexcept;
    [[nodiscard]] MemoryEventHandler memoryEventHandler() noexcept;

    void memoryEvent(MemoryEvent event,
                     const char *tag,
                     void *ptr,
                     void *oldPtr,
                     size_t bytes,
                     uint32_t caps) noexcept;
}

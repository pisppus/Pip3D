#include <PipCore/Debug/MemoryHooks.hpp>

namespace pipcore::debug
{
    namespace
    {
        void defaultMemoryEventHandler(MemoryEvent,
                                       const char *,
                                       void *,
                                       void *,
                                       size_t,
                                       uint32_t) noexcept
        {
        }

        MemoryEventHandler g_memoryEventHandler = defaultMemoryEventHandler;
    }

    void setMemoryEventHandler(MemoryEventHandler handler) noexcept
    {
        g_memoryEventHandler = handler ? handler : defaultMemoryEventHandler;
    }

    MemoryEventHandler memoryEventHandler() noexcept
    {
        return g_memoryEventHandler;
    }

    void memoryEvent(MemoryEvent event,
                     const char *tag,
                     void *ptr,
                     void *oldPtr,
                     size_t bytes,
                     uint32_t caps) noexcept
    {
        g_memoryEventHandler(event, tag, ptr, oldPtr, bytes, caps);
    }
}

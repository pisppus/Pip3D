#include <PipCore/Debug/MemoryHooks.hpp>

namespace pipcore::debug
{
    MemoryEventHandler g_memoryEventHandler = nullptr;

    void setMemoryEventHandler(MemoryEventHandler handler) noexcept
    {
        g_memoryEventHandler = handler;
    }

    MemoryEventHandler memoryEventHandler() noexcept
    {
        return g_memoryEventHandler;
    }
}
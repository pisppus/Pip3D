#include <PipCore/Debug/Memory.hpp>

namespace pipcore::debug
{
    MemoryHandler g_memoryHandler = nullptr;

    void setMemoryHandler(MemoryHandler handler) noexcept
    {
        g_memoryHandler = handler;
    }

    MemoryHandler memoryHandler() noexcept
    {
        return g_memoryHandler;
    }
}
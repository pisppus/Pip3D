#pragma once

#include <PipCore/Features.hpp>

namespace pipcore::debug
{
    class Server
    {
    public:
        static Server &instance() noexcept;
        void begin() noexcept;

    private:
        Server() = default;
        bool _started = false;
    };
}
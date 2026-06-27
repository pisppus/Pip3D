#pragma once

#include <PipCore/Displays/StDriver.hpp>

namespace pipcore::st7796
{
    using Driver = pipcore::detail::StDriver<pipcore::detail::StDisplayType::ST7796>;
    using Transport = pipcore::st::Transport;
    using IoError = pipcore::st::IoError;
    using pipcore::st::bswap16;
    using pipcore::st::copySwap565;
    using pipcore::st::ioErrorText;
}
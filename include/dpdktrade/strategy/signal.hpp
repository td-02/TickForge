#pragma once

#include <cstdint>

namespace dpdktrade::strategy
{
enum class Signal : std::uint8_t
{
    NO_SIGNAL = 0,
    BUY = 1,
    SELL = 2,
};
} // namespace dpdktrade::strategy

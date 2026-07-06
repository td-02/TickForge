#pragma once

#include <cstdint>

#include <dpdktrade/book/order_book.hpp>

namespace dpdktrade::strategy
{
enum class Signal : std::uint8_t
{
    NO_SIGNAL = 0,
    BUY = 1,
    SELL = 2,
};

// Cross multiplication avoids floating-point math and division in the hot path.
//
// BUY condition:
//     bid_pressure > ask_pressure * 1.10
// is rewritten as:
//     bid_pressure * 10 > ask_pressure * 11
//
// SELL condition:
//     ask_pressure > bid_pressure * 1.10
// is rewritten as:
//     ask_pressure * 10 > bid_pressure * 11
//
// The integer formulation is deterministic, branch-friendly, and keeps the
// strategy free from rounding behavior.
[[nodiscard]] constexpr Signal imbalance_signal(const book::OrderBook& order_book) noexcept
{
    const std::uint64_t bid = order_book.bid_pressure();
    const std::uint64_t ask = order_book.ask_pressure();

    if (bid * 10ULL > ask * 11ULL)
    {
        return Signal::BUY;
    }

    if (ask * 10ULL > bid * 11ULL)
    {
        return Signal::SELL;
    }

    return Signal::NO_SIGNAL;
}
} // namespace dpdktrade::strategy

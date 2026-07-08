#pragma once

#include <cstdint>

#include <dpdktrade/book/order_book.hpp>
#include <dpdktrade/strategy/signal.hpp>

namespace dpdktrade::strategy
{
struct ImbalanceStrategy final
{
    [[nodiscard]] constexpr Signal evaluate(const book::OrderBook& order_book) noexcept
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
};

[[nodiscard]] constexpr Signal imbalance_signal(const book::OrderBook& order_book) noexcept
{
    return ImbalanceStrategy{}.evaluate(order_book);
}
} // namespace dpdktrade::strategy

#pragma once

#include <cstddef>
#include <cstdint>

#include <dpdktrade/book/order_book.hpp>
#include <dpdktrade/strategy/signal.hpp>

namespace dpdktrade::strategy
{
struct MeanReversionStrategy final
{
    static constexpr std::size_t history_size = 4;

    [[nodiscard]] constexpr Signal evaluate(const book::OrderBook& order_book) noexcept
    {
        const auto best_bid = order_book.best_bid();
        const auto best_ask = order_book.best_ask();

        if (best_bid.quantity == 0 || best_ask.quantity == 0)
        {
            return Signal::NO_SIGNAL;
        }

        if (best_bid.price >= best_ask.price)
        {
            return Signal::NO_SIGNAL;
        }

        const std::uint64_t mid = (best_bid.price + best_ask.price) / 2ULL;
        history_[history_index_] = mid;
        history_index_ = (history_index_ + 1U) % history_size;
        if (filled_ < history_size)
        {
            ++filled_;
        }

        if (filled_ < history_size)
        {
            return Signal::NO_SIGNAL;
        }

        std::uint64_t average = 0;
        for (std::size_t index = 0; index < history_size; ++index)
        {
            average += history_[index];
        }
        average /= history_size;

        if (mid * 100ULL < average * 99ULL)
        {
            return Signal::BUY;
        }

        if (mid * 100ULL > average * 101ULL)
        {
            return Signal::SELL;
        }

        return Signal::NO_SIGNAL;
    }

private:
    std::uint64_t history_[history_size]{};
    std::size_t history_index_ = 0;
    std::size_t filled_ = 0;
};
} // namespace dpdktrade::strategy

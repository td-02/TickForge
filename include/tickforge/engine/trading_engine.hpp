#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <tickforge/book/order_book.hpp>
#include <tickforge/risk/guard.hpp>
#include <tickforge/strategy/imbalance.hpp>
#include <tickforge/wire/frame.hpp>

namespace tickforge::engine
{
class TradingEngine final
{
public:
    struct Statistics final
    {
        std::uint64_t total = 0;
        std::uint64_t buy = 0;
        std::uint64_t sell = 0;
        std::uint64_t no_signal = 0;
        std::uint64_t risk_reject = 0;
        std::uint64_t invalid = 0;
    };

    constexpr TradingEngine(book::OrderBook order_book, risk::RiskGuard risk_guard) noexcept
        : order_book_{order_book}
        , risk_guard_{risk_guard}
    {
    }

    [[nodiscard]] constexpr const Statistics& stats() const noexcept
    {
        return stats_;
    }

    [[nodiscard]] constexpr const book::OrderBook& order_book() const noexcept
    {
        return order_book_;
    }

    // Pipeline:
    // 1. Validate the market frame ethertype.
    // 2. Update the fixed-size order book.
    // 3. Generate a signal from current book state.
    // 4. Run risk checks before any order leaves the engine.
    // 5. Encode the order frame into a deterministic fixed-size wire image.
    //
    // The method returns std::nullopt on invalid input, no signal, or risk rejection.
    // That keeps the hot path free of heap allocations and avoids dynamic containers.
    [[nodiscard]] constexpr std::optional<wire::OrderFrame> on_market(const wire::MarketFrame& market_frame) noexcept
    {
        ++stats_.total;

        if (market_frame.ethertype != wire::ETHERTYPE_MARKET) [[unlikely]]
        {
            ++stats_.invalid;
            return std::nullopt;
        }

        const auto side = static_cast<book::OrderBook::Side>(market_frame.payload[0] & 0x01U);
        const std::uint64_t price = read_u64(market_frame.payload, 1);
        const std::uint64_t quantity = read_u64(market_frame.payload, 9);

        order_book_.apply(side, price, quantity);

        const strategy::Signal signal = strategy::imbalance_signal(order_book_);
        if (signal == strategy::Signal::NO_SIGNAL) [[likely]]
        {
            ++stats_.no_signal;
            return std::nullopt;
        }

        const bool is_buy = signal == strategy::Signal::BUY;
        if (is_buy)
        {
            ++stats_.buy;
        }
        else
        {
            ++stats_.sell;
        }

        const std::int64_t signed_quantity = is_buy ? static_cast<std::int64_t>(quantity)
                                                    : -static_cast<std::int64_t>(quantity);

        if (!risk_guard_.check_and_update(signed_quantity, price)) [[unlikely]]
        {
            ++stats_.risk_reject;
            return std::nullopt;
        }

        wire::OrderFrame order_frame{};
        order_frame.ethertype = wire::ETHERTYPE_ORDER;
        order_frame.payload[0] = is_buy ? 1U : 2U;
        write_u64(order_frame.payload, 1, price);
        write_u64(order_frame.payload, 9, quantity);
        return order_frame;
    }

private:
    book::OrderBook order_book_{};
    risk::RiskGuard risk_guard_;
    Statistics stats_{};

    [[nodiscard]] static constexpr std::uint64_t read_u64(const std::uint8_t* bytes, std::size_t offset) noexcept
    {
        std::uint64_t value = 0;
        for (std::size_t index = 0; index < sizeof(std::uint64_t); ++index)
        {
            value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8U);
        }
        return value;
    }

    static constexpr void write_u64(std::uint8_t* bytes, std::size_t offset, std::uint64_t value) noexcept
    {
        for (std::size_t index = 0; index < sizeof(std::uint64_t); ++index)
        {
            bytes[offset + index] = static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU);
        }
    }
};
} // namespace tickforge::engine

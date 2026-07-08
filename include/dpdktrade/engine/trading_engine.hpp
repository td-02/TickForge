#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <dpdktrade/book/order_book.hpp>
#include <dpdktrade/risk/guard.hpp>
#include <dpdktrade/strategy/imbalance.hpp>
#include <dpdktrade/strategy/mean_reversion.hpp>
#include <dpdktrade/strategy/momentum.hpp>
#include <dpdktrade/strategy/signal.hpp>
#include <dpdktrade/wire/frame.hpp>

namespace dpdktrade::engine
{
enum class StrategyMode : std::uint8_t
{
    Imbalance = 0,
    Momentum = 1,
    MeanReversion = 2,
};

template <StrategyMode Mode>
struct StrategySelector;

template <>
struct StrategySelector<StrategyMode::Imbalance> final
{
    using Strategy = dpdktrade::strategy::ImbalanceStrategy;
};

template <>
struct StrategySelector<StrategyMode::Momentum> final
{
    using Strategy = dpdktrade::strategy::MomentumStrategy;
};

template <>
struct StrategySelector<StrategyMode::MeanReversion> final
{
    using Strategy = dpdktrade::strategy::MeanReversionStrategy;
};

template <StrategyMode Mode = StrategyMode::Imbalance>
class TradingEngine final
{
public:
    using Strategy = typename StrategySelector<Mode>::Strategy;

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

        const strategy::Signal signal = strategy_.evaluate(order_book_);
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
    Strategy strategy_{};
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

template <StrategyMode Mode = StrategyMode::Imbalance>
using DpdkTradeEngine = TradingEngine<Mode>;
} // namespace dpdktrade::engine

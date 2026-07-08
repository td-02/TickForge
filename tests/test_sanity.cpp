#include <cassert>
#include <cstdint>

#include <dpdktrade/book/order_book.hpp>
#include <dpdktrade/engine/dpdktrade_engine.hpp>
#include <dpdktrade/risk/guard.hpp>
#include <dpdktrade/strategy/imbalance.hpp>
#include <dpdktrade/strategy/mean_reversion.hpp>
#include <dpdktrade/strategy/momentum.hpp>
#include <dpdktrade/wire/frame.hpp>

namespace
{
[[nodiscard]] constexpr dpdktrade::book::OrderBook make_book(std::uint64_t bid_price,
                                                             std::uint64_t bid_quantity,
                                                             std::uint64_t ask_price,
                                                             std::uint64_t ask_quantity) noexcept
{
    dpdktrade::book::OrderBook order_book{};
    order_book.apply(dpdktrade::book::OrderBook::Side::Bid, bid_price, bid_quantity);
    order_book.apply(dpdktrade::book::OrderBook::Side::Ask, ask_price, ask_quantity);
    return order_book;
}
} // namespace

int main()
{
    using namespace dpdktrade;

    // Foundation test: ensures the test target builds and runs cleanly.
    assert(true);

    const auto imbalance_buy_book = make_book(100U, 220U, 101U, 100U);
    const auto imbalance_sell_book = make_book(100U, 100U, 101U, 220U);
    assert(strategy::imbalance_signal(imbalance_buy_book) == strategy::Signal::BUY);
    assert(strategy::imbalance_signal(imbalance_sell_book) == strategy::Signal::SELL);

    strategy::MomentumStrategy momentum_strategy{};
    strategy::MeanReversionStrategy mean_reversion_strategy{};
    for (int index = 0; index < 4; ++index)
    {
        (void)momentum_strategy.evaluate(make_book(100U + static_cast<std::uint64_t>(index),
                                                   10U,
                                                   101U + static_cast<std::uint64_t>(index),
                                                   10U));
        (void)mean_reversion_strategy.evaluate(make_book(100U + static_cast<std::uint64_t>(index),
                                                         10U,
                                                         110U + static_cast<std::uint64_t>(index),
                                                         10U));
    }
    assert(momentum_strategy.evaluate(make_book(120U, 10U, 121U, 10U)) != strategy::Signal::NO_SIGNAL);
    assert(mean_reversion_strategy.evaluate(make_book(90U, 10U, 100U, 10U)) != strategy::Signal::NO_SIGNAL);

    engine::DpdkTradeEngine<> imbalance_engine{book::OrderBook{}, risk::RiskGuard{{1000, 100000U}}};
    wire::MarketFrame market_frame{};
    market_frame.ethertype = wire::ETHERTYPE_MARKET;
    market_frame.payload[0] = 0U;
    market_frame.payload[1] = 0x64U;
    market_frame.payload[9] = 0xC8U;

    const auto result = imbalance_engine.on_market(market_frame);
    assert(result.has_value());
    assert(imbalance_engine.stats().total == 1U);

    return 0;
}

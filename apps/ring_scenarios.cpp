#include <cstddef>
#include <cstdint>
#include <iostream>

#include <dpdktrade/book/order_book.hpp>
#include <dpdktrade/engine/trading_engine.hpp>
#include <dpdktrade/risk/guard.hpp>
#include <dpdktrade/strategy/imbalance.hpp>
#include <dpdktrade/strategy/mean_reversion.hpp>
#include <dpdktrade/strategy/momentum.hpp>
#include <dpdktrade/wire/frame.hpp>

#if defined(__has_include)
#    if __has_include(<rte_ring.h>) && __has_include(<rte_malloc.h>)
#        include <rte_ring.h>
#        include <rte_malloc.h>
#        define DPDKTRADE_HAS_DPDK 1
#    endif
#endif

#ifndef DPDKTRADE_HAS_DPDK
#    define DPDKTRADE_HAS_DPDK 0
#endif

namespace
{
using namespace dpdktrade;

struct ScenarioResult final
{
    bool buy = false;
    bool sell = false;
    bool no_signal = false;
    bool risk_reject = false;
    bool invalid_frame = false;
};

[[nodiscard]] wire::MarketFrame make_market_frame(std::uint8_t side, std::uint64_t price, std::uint64_t quantity) noexcept
{
    wire::MarketFrame frame{};
    frame.ethertype = wire::ETHERTYPE_MARKET;
    frame.payload[0] = side;
    for (std::size_t index = 0; index < sizeof(std::uint64_t); ++index)
    {
        frame.payload[1 + index] = static_cast<std::uint8_t>((price >> (index * 8U)) & 0xFFU);
        frame.payload[9 + index] = static_cast<std::uint8_t>((quantity >> (index * 8U)) & 0xFFU);
    }
    return frame;
}

template <typename Engine>
[[nodiscard]] ScenarioResult run_scenarios() noexcept
{
    ScenarioResult result{};

    {
        Engine engine{book::OrderBook{}, risk::RiskGuard{{1000, 100000U}}};
        const auto output = engine.on_market(make_market_frame(0U, 100U, 220U));
        result.buy = output.has_value();
    }

    {
        Engine engine{book::OrderBook{}, risk::RiskGuard{{1000, 100000U}}};
        const auto output = engine.on_market(make_market_frame(1U, 100U, 220U));
        result.sell = output.has_value();
    }

    {
        Engine engine{book::OrderBook{}, risk::RiskGuard{{1000, 100000U}}};
        const auto output = engine.on_market(make_market_frame(0U, 100U, 0U));
        result.no_signal = !output.has_value();
    }

    {
        Engine engine{book::OrderBook{}, risk::RiskGuard{{1, 1U}}};
        const auto output = engine.on_market(make_market_frame(0U, 100U, 220U));
        result.risk_reject = !output.has_value();
    }

    {
        Engine engine{book::OrderBook{}, risk::RiskGuard{{1000, 100000U}}};
        wire::MarketFrame invalid{};
        invalid.ethertype = 0xFFFFU;
        const auto output = engine.on_market(invalid);
        result.invalid_frame = !output.has_value();
    }

    return result;
}

#if DPDKTRADE_HAS_DPDK
[[nodiscard]] rte_ring* create_ring(const char* name) noexcept
{
    constexpr unsigned capacity = 1024;
    return rte_ring_create(name, capacity, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
}
#endif
} // namespace

int main()
{
    const ScenarioResult imbalance = run_scenarios<engine::TradingEngine<engine::StrategyMode::Imbalance>>();
    const ScenarioResult momentum = run_scenarios<engine::TradingEngine<engine::StrategyMode::Momentum>>();
    const ScenarioResult mean_reversion = run_scenarios<engine::TradingEngine<engine::StrategyMode::MeanReversion>>();

    std::cout << "imbalance BUY: " << (imbalance.buy ? "PASS" : "FAIL") << '\n';
    std::cout << "imbalance SELL: " << (imbalance.sell ? "PASS" : "FAIL") << '\n';
    std::cout << "imbalance NO_SIGNAL: " << (imbalance.no_signal ? "PASS" : "FAIL") << '\n';
    std::cout << "imbalance RISK_REJECT: " << (imbalance.risk_reject ? "PASS" : "FAIL") << '\n';
    std::cout << "imbalance INVALID_FRAME: " << (imbalance.invalid_frame ? "PASS" : "FAIL") << '\n';
    std::cout << "momentum BUY: " << (momentum.buy ? "PASS" : "FAIL") << '\n';
    std::cout << "momentum SELL: " << (momentum.sell ? "PASS" : "FAIL") << '\n';
    std::cout << "momentum NO_SIGNAL: " << (momentum.no_signal ? "PASS" : "FAIL") << '\n';
    std::cout << "momentum RISK_REJECT: " << (momentum.risk_reject ? "PASS" : "FAIL") << '\n';
    std::cout << "momentum INVALID_FRAME: " << (momentum.invalid_frame ? "PASS" : "FAIL") << '\n';
    std::cout << "mean_reversion BUY: " << (mean_reversion.buy ? "PASS" : "FAIL") << '\n';
    std::cout << "mean_reversion SELL: " << (mean_reversion.sell ? "PASS" : "FAIL") << '\n';
    std::cout << "mean_reversion NO_SIGNAL: " << (mean_reversion.no_signal ? "PASS" : "FAIL") << '\n';
    std::cout << "mean_reversion RISK_REJECT: " << (mean_reversion.risk_reject ? "PASS" : "FAIL") << '\n';
    std::cout << "mean_reversion INVALID_FRAME: " << (mean_reversion.invalid_frame ? "PASS" : "FAIL") << '\n';

#if DPDKTRADE_HAS_DPDK
    if (rte_ring* ring = create_ring("dpdktrade_ring_validation"); ring != nullptr)
    {
        rte_ring_free(ring);
    }
#endif

    return (imbalance.buy && imbalance.sell && imbalance.no_signal && imbalance.risk_reject && imbalance.invalid_frame &&
            momentum.buy && momentum.sell && momentum.no_signal && momentum.risk_reject && momentum.invalid_frame &&
            mean_reversion.buy && mean_reversion.sell && mean_reversion.no_signal && mean_reversion.risk_reject &&
            mean_reversion.invalid_frame)
               ? 0
               : 1;
}

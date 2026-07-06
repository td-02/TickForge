#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include <dpdktrade/book/order_book.hpp>
#include <dpdktrade/engine/dpdktrade_engine.hpp>
#include <dpdktrade/risk/guard.hpp>
#include <dpdktrade/strategy/imbalance.hpp>
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

[[nodiscard]] ScenarioResult run_scenarios() noexcept
{
    ScenarioResult result{};

    {
        engine::DpdkTradeEngine engine{book::OrderBook{}, risk::RiskGuard{{1000, 100000U}}};
        const auto output = engine.on_market(make_market_frame(0U, 100U, 220U));
        result.buy = output.has_value();
    }

    {
        engine::DpdkTradeEngine engine{book::OrderBook{}, risk::RiskGuard{{1000, 100000U}}};
        const auto output = engine.on_market(make_market_frame(1U, 100U, 220U));
        result.sell = output.has_value();
    }

    {
        engine::DpdkTradeEngine engine{book::OrderBook{}, risk::RiskGuard{{1000, 100000U}}};
        const auto output = engine.on_market(make_market_frame(0U, 100U, 0U));
        result.no_signal = !output.has_value();
    }

    {
        engine::DpdkTradeEngine engine{book::OrderBook{}, risk::RiskGuard{{1, 1U}}};
        const auto output = engine.on_market(make_market_frame(0U, 100U, 220U));
        result.risk_reject = !output.has_value();
    }

    {
        engine::DpdkTradeEngine engine{book::OrderBook{}, risk::RiskGuard{{1000, 100000U}}};
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
    rte_ring* ring = rte_ring_create(name, capacity, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    return ring;
}
#endif
} // namespace

int main()
{
    const ScenarioResult result = run_scenarios();

    std::cout << "BUY: " << (result.buy ? "PASS" : "FAIL") << '\n';
    std::cout << "SELL: " << (result.sell ? "PASS" : "FAIL") << '\n';
    std::cout << "NO_SIGNAL: " << (result.no_signal ? "PASS" : "FAIL") << '\n';
    std::cout << "RISK_REJECT: " << (result.risk_reject ? "PASS" : "FAIL") << '\n';
    std::cout << "INVALID_FRAME: " << (result.invalid_frame ? "PASS" : "FAIL") << '\n';

#if DPDKTRADE_HAS_DPDK
    if (rte_ring* ring = create_ring("dpdktrade_ring_validation"); ring != nullptr)
    {
        rte_ring_free(ring);
    }
#endif

    return (result.buy && result.sell && result.no_signal && result.risk_reject && result.invalid_frame) ? 0 : 1;
}

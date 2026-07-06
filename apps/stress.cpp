#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include <dpdktrade/book/order_book.hpp>
#include <dpdktrade/engine/dpdktrade_engine.hpp>
#include <dpdktrade/risk/guard.hpp>
#include <dpdktrade/wire/frame.hpp>

#if defined(__has_include)
#    if __has_include(<rte_cycles.h>) && __has_include(<rte_eal.h>) && __has_include(<rte_ethdev.h>) && __has_include(<rte_ring.h>)
#        include <rte_cycles.h>
#        include <rte_eal.h>
#        include <rte_ethdev.h>
#        include <rte_ring.h>
#        define DPDKTRADE_HAS_DPDK 1
#    endif
#endif

#ifndef DPDKTRADE_HAS_DPDK
#    define DPDKTRADE_HAS_DPDK 0
#endif

namespace
{
[[nodiscard]] constexpr dpdktrade::wire::MarketFrame make_market_frame(std::uint8_t side, std::uint64_t price, std::uint64_t quantity) noexcept
{
    dpdktrade::wire::MarketFrame frame{};
    frame.ethertype = dpdktrade::wire::ETHERTYPE_MARKET;
    frame.payload[0] = side;

    for (std::size_t index = 0; index < sizeof(std::uint64_t); ++index)
    {
        frame.payload[1 + index] = static_cast<std::uint8_t>((price >> (index * 8U)) & 0xFFU);
        frame.payload[9 + index] = static_cast<std::uint8_t>((quantity >> (index * 8U)) & 0xFFU);
    }

    return frame;
}

[[nodiscard]] bool run_engine_stress(std::size_t iterations) noexcept
{
    dpdktrade::engine::DpdkTradeEngine engine{dpdktrade::book::OrderBook{}, dpdktrade::risk::RiskGuard{{1'000'000, 10'000'000U}}};

    const std::array<dpdktrade::wire::MarketFrame, 4> frames{
        make_market_frame(0U, 100U, 220U),
        make_market_frame(1U, 101U, 180U),
        make_market_frame(0U, 102U, 0U),
        [] {
            dpdktrade::wire::MarketFrame frame{};
            frame.ethertype = 0xFFFFU;
            return frame;
        }(),
    };

    std::size_t outputs = 0;
    for (std::size_t index = 0; index < iterations; ++index)
    {
        const auto& frame = frames[index % frames.size()];
        if (engine.on_market(frame).has_value())
        {
            ++outputs;
        }
    }

    const auto& stats = engine.stats();
    const std::size_t expected_invalid = iterations / frames.size();
    return stats.total == iterations && stats.invalid == expected_invalid && outputs > 0U;
}

#if DPDKTRADE_HAS_DPDK
[[nodiscard]] bool run_ring_stress(std::size_t iterations) noexcept
{
    rte_ring* ring = rte_ring_create("dpdktrade_stress_ring", 4096, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    if (ring == nullptr)
    {
        return false;
    }

    for (std::size_t index = 0; index < iterations; ++index)
    {
        void* payload = reinterpret_cast<void*>(index + 1U);
        if (rte_ring_enqueue(ring, payload) != 0)
        {
            rte_ring_free(ring);
            return false;
        }

        void* received = nullptr;
        if (rte_ring_dequeue(ring, &received) != 0 || received != payload)
        {
            rte_ring_free(ring);
            return false;
        }
    }

    rte_ring_free(ring);
    return true;
}

[[nodiscard]] bool run_afpacket_stress(std::size_t iterations) noexcept
{
    rte_ring* ring = rte_ring_create("dpdktrade_stress_afpacket", 4096, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    if (ring == nullptr)
    {
        return false;
    }

    const dpdktrade::wire::MarketFrame frame = make_market_frame(0U, 100U, 10U);
    for (std::size_t index = 0; index < iterations; ++index)
    {
        void* payload = const_cast<dpdktrade::wire::MarketFrame*>(reinterpret_cast<const dpdktrade::wire::MarketFrame*>(&frame));
        if (rte_ring_enqueue(ring, payload) != 0)
        {
            rte_ring_free(ring);
            return false;
        }

        void* received = nullptr;
        if (rte_ring_dequeue(ring, &received) != 0 || received != payload)
        {
            rte_ring_free(ring);
            return false;
        }
    }

    rte_ring_free(ring);
    return true;
}
#endif
} // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    constexpr std::size_t iterations = 1'000'000;

    const bool engine_ok = run_engine_stress(iterations);
    std::cout << "engine_stress: " << (engine_ok ? "PASS" : "FAIL") << '\n';

#if DPDKTRADE_HAS_DPDK
    const bool ring_ok = run_ring_stress(iterations);
    const bool afpacket_ok = run_afpacket_stress(iterations);
    std::cout << "ring_stress: " << (ring_ok ? "PASS" : "FAIL") << '\n';
    std::cout << "afpacket_stress: " << (afpacket_ok ? "PASS" : "FAIL") << '\n';
    return (engine_ok && ring_ok && afpacket_ok) ? 0 : 1;
#else
    std::cout << "ring_stress: SKIPPED (DPDK headers unavailable)\n";
    std::cout << "afpacket_stress: SKIPPED (DPDK headers unavailable)\n";
    return engine_ok ? 0 : 1;
#endif
}

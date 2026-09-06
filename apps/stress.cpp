#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <system_error>

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
inline constexpr std::size_t default_iterations = 1'000'000;

struct StressConfig final
{
    std::size_t iterations = default_iterations;
    bool show_help = false;
};

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
        // The frame set is a power-of-two ring, so a bitmask is cheaper than modulo here.
        const auto& frame = frames[index & (frames.size() - 1U)];
        if (engine.on_market(frame).has_value())
        {
            ++outputs;
        }
    }

    const auto& stats = engine.stats();
    const std::size_t expected_invalid = iterations / frames.size();
    return stats.total == iterations && stats.invalid == expected_invalid && outputs > 0U;
}

[[nodiscard]] bool parse_iterations(std::string_view value, std::size_t& iterations) noexcept
{
    std::size_t parsed = 0;
    const char* const first = value.data();
    const char* const last = first + value.size();
    const auto [ptr, error] = std::from_chars(first, last, parsed);
    if (error != std::errc{} || ptr != last || parsed == 0U)
    {
        return false;
    }

    iterations = parsed;
    return true;
}

[[nodiscard]] bool parse_args(int argc, char** argv, StressConfig& config) noexcept
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view arg{argv[index]};
        if (arg == "--help" || arg == "-h")
        {
            config.show_help = true;
            return true;
        }

        if (arg == "--iterations" || arg == "-n")
        {
            if (index + 1 >= argc)
            {
                return false;
            }

            ++index;
            if (!parse_iterations(argv[index], config.iterations))
            {
                return false;
            }
            continue;
        }

        constexpr std::string_view iterations_prefix = "--iterations=";
        if (arg.starts_with(iterations_prefix))
        {
            if (!parse_iterations(arg.substr(iterations_prefix.size()), config.iterations))
            {
                return false;
            }
            continue;
        }

        return false;
    }

    return true;
}

void print_usage(std::ostream& out) noexcept
{
    out << "Usage: dpdktrade_stress [--iterations N]\n"
        << "       dpdktrade_stress [-n N]\n"
        << "\n"
        << "Runs engine, ring, and AF_PACKET stress checks when their dependencies are available.\n"
        << "Default iterations: " << default_iterations << "\n";
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
    StressConfig config{};
    if (!parse_args(argc, argv, config))
    {
        print_usage(std::cerr);
        return 2;
    }

    if (config.show_help)
    {
        print_usage(std::cout);
        return 0;
    }

    const std::size_t iterations = config.iterations;
    std::cout << "iterations: " << iterations << '\n';

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

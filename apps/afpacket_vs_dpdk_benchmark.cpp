#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <vector>

#include <dpdktrade/engine/dpdktrade_engine.hpp>
#include <tickforge/dpdk/market_transport.hpp>

namespace
{
constexpr std::size_t event_count = 1'000'000;
constexpr std::size_t queue_capacity = 4096;

struct LatencyStats final
{
    std::uint64_t min = 0;
    std::uint64_t mean = 0;
    std::uint64_t p50 = 0;
    std::uint64_t p95 = 0;
    std::uint64_t p99 = 0;
    std::uint64_t max = 0;
};

struct BenchmarkResult final
{
    LatencyStats afpacket;
    std::optional<LatencyStats> dpdk;
};

struct LocalQueue final
{
    std::array<dpdktrade::wire::MarketFrame, queue_capacity> storage{};
    std::size_t head = 0;
    std::size_t tail = 0;
    std::size_t size = 0;

    [[nodiscard]] bool push(const dpdktrade::wire::MarketFrame& frame) noexcept
    {
        if (size == storage.size())
        {
            return false;
        }

        storage[tail] = frame;
        tail = (tail + 1U) % storage.size();
        ++size;
        return true;
    }

    [[nodiscard]] bool pop(dpdktrade::wire::MarketFrame& frame) noexcept
    {
        if (size == 0U)
        {
            return false;
        }

        frame = storage[head];
        head = (head + 1U) % storage.size();
        --size;
        return true;
    }
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

[[nodiscard]] constexpr std::array<dpdktrade::wire::MarketFrame, 4> make_load_pattern() noexcept
{
    return {
        make_market_frame(0U, 100U, 220U),
        make_market_frame(1U, 101U, 180U),
        make_market_frame(0U, 102U, 0U),
        make_market_frame(1U, 103U, 240U),
    };
}

[[nodiscard]] std::uint64_t percentile(std::vector<std::uint64_t>& samples, double ratio) noexcept
{
    const std::size_t index = static_cast<std::size_t>(ratio * static_cast<double>(samples.size() - 1U));
    std::nth_element(samples.begin(), samples.begin() + index, samples.end());
    return samples[index];
}

[[nodiscard]] LatencyStats compute_stats(std::vector<std::uint64_t>& samples) noexcept
{
    if (samples.empty())
    {
        return {};
    }

    const auto [min_it, max_it] = std::minmax_element(samples.begin(), samples.end());
    const std::uint64_t min = *min_it;
    const std::uint64_t max = *max_it;
    const std::uint64_t mean = static_cast<std::uint64_t>(
        std::accumulate(samples.begin(), samples.end(), std::uint64_t{0}) / static_cast<std::uint64_t>(samples.size()));

    return LatencyStats{
        .min = min,
        .mean = mean,
        .p50 = percentile(samples, 0.50),
        .p95 = percentile(samples, 0.95),
        .p99 = percentile(samples, 0.99),
        .max = max,
    };
}

[[nodiscard]] bool write_results(const BenchmarkResult& result) noexcept
{
    std::ofstream out("results/afpacket_vs_dpdk.csv", std::ios::out | std::ios::trunc);
    if (!out)
    {
        return false;
    }

    out << "metric,value\n";
    out << "afpacket_min_ns," << result.afpacket.min << "\n";
    out << "afpacket_mean_ns," << result.afpacket.mean << "\n";
    out << "afpacket_p50_ns," << result.afpacket.p50 << "\n";
    out << "afpacket_p95_ns," << result.afpacket.p95 << "\n";
    out << "afpacket_p99_ns," << result.afpacket.p99 << "\n";
    out << "afpacket_max_ns," << result.afpacket.max << "\n";

    if (result.dpdk.has_value())
    {
        out << "dpdk_min_ns," << result.dpdk->min << "\n";
        out << "dpdk_mean_ns," << result.dpdk->mean << "\n";
        out << "dpdk_p50_ns," << result.dpdk->p50 << "\n";
        out << "dpdk_p95_ns," << result.dpdk->p95 << "\n";
        out << "dpdk_p99_ns," << result.dpdk->p99 << "\n";
        out << "dpdk_max_ns," << result.dpdk->max << "\n";
    }
    else
    {
        out << "dpdk_status,skipped\n";
    }

    return true;
}

[[nodiscard]] LatencyStats run_afpacket_path() noexcept
{
    dpdktrade::engine::DpdkTradeEngine engine{dpdktrade::book::OrderBook{}, dpdktrade::risk::RiskGuard{{1000, 100000U}}};
    LocalQueue queue{};
    constexpr std::size_t pattern_mask = make_load_pattern().size() - 1U;
    const auto pattern = make_load_pattern();
    std::vector<std::uint64_t> samples;
    samples.reserve(event_count);

    for (std::size_t index = 0; index < event_count; ++index)
    {
        const auto start = std::chrono::steady_clock::now();
        const dpdktrade::wire::MarketFrame& frame = pattern[index & pattern_mask];
        if (!queue.push(frame))
        {
            break;
        }

        dpdktrade::wire::MarketFrame queued{};
        if (!queue.pop(queued))
        {
            break;
        }

        (void)engine.on_market(queued);
        const auto end = std::chrono::steady_clock::now();
        samples.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
    }

    return compute_stats(samples);
}

#if TICKFORGE_WITH_DPDK_HEADERS
[[nodiscard]] std::optional<LatencyStats> run_dpdk_path() noexcept
{
    tickforge::dpdk::MarketTransport transport{};
    if (!tickforge::dpdk::initialize_transport(transport, "afpacket_vs_dpdk_ring", "afpacket_vs_dpdk_pool"))
    {
        return std::nullopt;
    }

    dpdktrade::engine::DpdkTradeEngine engine{dpdktrade::book::OrderBook{}, dpdktrade::risk::RiskGuard{{1000, 100000U}}};
    constexpr std::size_t pattern_mask = make_load_pattern().size() - 1U;
    const auto pattern = make_load_pattern();
    std::vector<std::uint64_t> samples;
    samples.reserve(event_count);

    for (std::size_t index = 0; index < event_count; ++index)
    {
        const auto start = std::chrono::steady_clock::now();
        const dpdktrade::wire::MarketFrame& frame = pattern[index & pattern_mask];
        if (!tickforge::dpdk::push_frame(transport, frame))
        {
            tickforge::dpdk::shutdown_transport(transport);
            return std::nullopt;
        }

        dpdktrade::wire::MarketFrame* queued = nullptr;
        if (!tickforge::dpdk::pop_frame(transport, queued))
        {
            tickforge::dpdk::shutdown_transport(transport);
            return std::nullopt;
        }

        (void)engine.on_market(*queued);
        tickforge::dpdk::release_frame(transport, queued);
        const auto end = std::chrono::steady_clock::now();
        samples.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
    }

    tickforge::dpdk::shutdown_transport(transport);
    return compute_stats(samples);
}
#else
[[nodiscard]] std::optional<LatencyStats> run_dpdk_path() noexcept
{
    return std::nullopt;
}
#endif
} // namespace

int main(int argc, char** argv)
{
#if TICKFORGE_WITH_DPDK_HEADERS
    const char* eal_args[] = {argv[0], "-l", "0", "-n", "4"};
    const int eal_argc = static_cast<int>(sizeof(eal_args) / sizeof(eal_args[0]));
    if (rte_eal_init(eal_argc, const_cast<char**>(eal_args)) < 0)
    {
        std::cerr << "Failed to initialize DPDK EAL.\n";
        return 1;
    }
#else
    (void)argc;
    (void)argv;
#endif

    const LatencyStats afpacket_stats = run_afpacket_path();
    const std::optional<LatencyStats> dpdk_stats = run_dpdk_path();

    std::cout << "afpacket p50 ns: " << afpacket_stats.p50 << '\n';
    std::cout << "afpacket p95 ns: " << afpacket_stats.p95 << '\n';
    std::cout << "afpacket p99 ns: " << afpacket_stats.p99 << '\n';
    std::cout << "afpacket mean ns: " << afpacket_stats.mean << '\n';

    if (dpdk_stats.has_value())
    {
        std::cout << "dpdk p50 ns: " << dpdk_stats->p50 << '\n';
        std::cout << "dpdk p95 ns: " << dpdk_stats->p95 << '\n';
        std::cout << "dpdk p99 ns: " << dpdk_stats->p99 << '\n';
        std::cout << "dpdk mean ns: " << dpdk_stats->mean << '\n';
    }
    else
    {
        std::cout << "dpdk path skipped\n";
    }

    const BenchmarkResult result{afpacket_stats, dpdk_stats};
    if (!write_results(result))
    {
        std::cerr << "Failed to write benchmark results.\n";
        return 1;
    }

    return 0;
}

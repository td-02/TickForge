#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include <dpdktrade/wire/frame.hpp>

#if defined(__has_include)
#    if __has_include(<rte_cycles.h>) && __has_include(<rte_eal.h>) && __has_include(<rte_ethdev.h>)
#        include <rte_cycles.h>
#        include <rte_eal.h>
#        include <rte_ethdev.h>
#        define DPDKTRADE_HAS_DPDK 1
#    endif
#endif

#ifndef DPDKTRADE_HAS_DPDK
#    define DPDKTRADE_HAS_DPDK 0
#endif

namespace
{
struct BenchmarkResult final
{
    std::uint64_t p50_cycles = 0;
    std::uint64_t p95_cycles = 0;
    std::uint64_t p99_cycles = 0;
    double throughput_mpps = 0.0;
};

[[nodiscard]] constexpr dpdktrade::wire::MarketFrame make_market_frame(std::uint64_t price, std::uint64_t quantity) noexcept
{
    dpdktrade::wire::MarketFrame frame{};
    frame.ethertype = dpdktrade::wire::ETHERTYPE_MARKET;
    frame.payload[0] = 0U;

    for (std::size_t index = 0; index < sizeof(std::uint64_t); ++index)
    {
        frame.payload[1 + index] = static_cast<std::uint8_t>((price >> (index * 8U)) & 0xFFU);
        frame.payload[9 + index] = static_cast<std::uint8_t>((quantity >> (index * 8U)) & 0xFFU);
    }

    return frame;
}

[[nodiscard]] std::uint64_t percentile(std::vector<std::uint64_t>& samples, double fraction) noexcept
{
    const std::size_t index = static_cast<std::size_t>(fraction * static_cast<double>(samples.size() - 1U));
    std::nth_element(samples.begin(), samples.begin() + index, samples.end());
    return samples[index];
}

[[nodiscard]] BenchmarkResult compute_result(std::vector<std::uint64_t>& samples, double elapsed_seconds) noexcept
{
    BenchmarkResult result{};
    result.p50_cycles = percentile(samples, 0.50);
    result.p95_cycles = percentile(samples, 0.95);
    result.p99_cycles = percentile(samples, 0.99);
    result.throughput_mpps = (static_cast<double>(samples.size()) / elapsed_seconds) / 1'000'000.0;
    return result;
}

[[nodiscard]] bool write_csv(const BenchmarkResult& result, const char* path) noexcept
{
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out)
    {
        return false;
    }

    out << "metric,value\n";
    out << "p50_cycles," << result.p50_cycles << "\n";
    out << "p95_cycles," << result.p95_cycles << "\n";
    out << "p99_cycles," << result.p99_cycles << "\n";
    out << "throughput_mpps," << result.throughput_mpps << "\n";
    return true;
}

[[nodiscard]] bool write_report(const BenchmarkResult& result, const char* path) noexcept
{
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out)
    {
        return false;
    }

    out << "# Trading Engine AF_PACKET Benchmark Report\n\n";
    out << "- p50 cycles: " << result.p50_cycles << "\n";
    out << "- p95 cycles: " << result.p95_cycles << "\n";
    out << "- p99 cycles: " << result.p99_cycles << "\n";
    out << "- throughput Mpps: " << result.throughput_mpps << "\n";
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    constexpr std::size_t message_count = 1'000'000;
    const auto frame = make_market_frame(100U, 10U);

#if !DPDKTRADE_HAS_DPDK
    (void)message_count;
    std::cout << "DPDK headers are not available; AF_PACKET benchmark skipped.\n";
    std::cout << "MarketFrame ethertype: " << frame.ethertype << "\n";
    return 0;
#else
    const char* eal_args[] = {argv[0], "-l", "0", "-n", "4"};
    if (rte_eal_init(4, const_cast<char**>(eal_args)) < 0)
    {
        std::cerr << "Failed to initialize DPDK EAL.\n";
        return 1;
    }

    std::vector<std::uint64_t> samples;
    samples.reserve(message_count);

    rte_ring* ring = rte_ring_create("dpdktrade_afpacket_benchmark", 4096, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    if (ring == nullptr)
    {
        std::cerr << "Failed to create DPDK ring.\n";
        return 1;
    }

    const std::uint64_t start_cycles = rte_rdtsc();
    for (std::size_t index = 0; index < message_count; ++index)
    {
        const std::uint64_t before = rte_rdtsc();
        void* payload = const_cast<dpdktrade::wire::MarketFrame*>(reinterpret_cast<const dpdktrade::wire::MarketFrame*>(&frame));
        rte_ring_enqueue(ring, payload);
        void* received = nullptr;
        rte_ring_dequeue(ring, &received);
        const std::uint64_t after = rte_rdtsc();
        samples.push_back(after - before);
    }
    const std::uint64_t end_cycles = rte_rdtsc();

    const double elapsed_seconds = static_cast<double>(end_cycles - start_cycles) / static_cast<double>(rte_get_tsc_hz());
    const BenchmarkResult result = compute_result(samples, elapsed_seconds);

    const bool csv_ok = write_csv(result, "results/afpacket_benchmark.csv");
    const bool report_ok = write_report(result, "results/afpacket_benchmark.md");

    std::cout << "p50 cycles: " << result.p50_cycles << "\n";
    std::cout << "p95 cycles: " << result.p95_cycles << "\n";
    std::cout << "p99 cycles: " << result.p99_cycles << "\n";
    std::cout << "throughput Mpps: " << result.throughput_mpps << "\n";

    rte_ring_free(ring);
    return (csv_ok && report_ok) ? 0 : 1;
#endif
}

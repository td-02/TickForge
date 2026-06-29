#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if defined(__has_include)
#    if __has_include(<rte_cycles.h>) && __has_include(<rte_ring.h>)
#        include <rte_cycles.h>
#        include <rte_ring.h>
#        define TICKFORGE_HAS_DPDK 1
#    endif
#endif

#ifndef TICKFORGE_HAS_DPDK
#    define TICKFORGE_HAS_DPDK 0
#endif

namespace
{
struct BenchmarkStats final
{
    std::uint64_t p50 = 0;
    std::uint64_t p95 = 0;
    std::uint64_t p99 = 0;
    double throughput_mpps = 0.0;
};

[[nodiscard]] BenchmarkStats compute_stats(std::vector<std::uint64_t> samples, double elapsed_seconds) noexcept
{
    std::sort(samples.begin(), samples.end());

    const auto percentile = [&samples](double ratio) -> std::uint64_t {
        const std::size_t index = static_cast<std::size_t>(ratio * static_cast<double>(samples.size() - 1));
        return samples[index];
    };

    BenchmarkStats stats{};
    stats.p50 = percentile(0.50);
    stats.p95 = percentile(0.95);
    stats.p99 = percentile(0.99);
    stats.throughput_mpps = (samples.size() / elapsed_seconds) / 1'000'000.0;
    return stats;
}

[[nodiscard]] bool write_results(const BenchmarkStats& stats) noexcept
{
    std::ofstream out("results/ring_benchmark.csv", std::ios::out | std::ios::trunc);
    if (!out)
    {
        return false;
    }

    out << "metric,value\n";
    out << "p50_cycles," << stats.p50 << "\n";
    out << "p95_cycles," << stats.p95 << "\n";
    out << "p99_cycles," << stats.p99 << "\n";
    out << "throughput_mpps," << stats.throughput_mpps << "\n";
    return true;
}
} // namespace

int main()
{
#if !TICKFORGE_HAS_DPDK
    std::cerr << "DPDK headers are not available; ring benchmark is disabled.\n";
    return 0;
#else
    constexpr std::size_t event_count = 1'000'000;
    std::vector<std::uint64_t> samples;
    samples.reserve(event_count);

    rte_ring* ring = rte_ring_create("tickforge_benchmark_ring", 1024, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    if (ring == nullptr)
    {
        std::cerr << "Failed to create rte_ring.\n";
        return 1;
    }

    const std::uint64_t start_cycles = rte_rdtsc();

    for (std::size_t index = 0; index < event_count; ++index)
    {
        const std::uint64_t before = rte_rdtsc();
        const void* payload = reinterpret_cast<const void*>(index + 1);
        rte_ring_enqueue(ring, const_cast<void*>(payload));
        void* received = nullptr;
        rte_ring_dequeue(ring, &received);
        const std::uint64_t after = rte_rdtsc();
        samples.push_back(after - before);
    }

    const std::uint64_t end_cycles = rte_rdtsc();
    const double elapsed_cycles = static_cast<double>(end_cycles - start_cycles);
    const double cycles_per_second = static_cast<double>(rte_get_tsc_hz());
    const double elapsed_seconds = elapsed_cycles / cycles_per_second;

    const BenchmarkStats stats = compute_stats(std::move(samples), elapsed_seconds);

    if (!write_results(stats))
    {
        std::cerr << "Failed to write benchmark results.\n";
        rte_ring_free(ring);
        return 1;
    }

    std::cout << "p50 cycles: " << stats.p50 << '\n';
    std::cout << "p95 cycles: " << stats.p95 << '\n';
    std::cout << "p99 cycles: " << stats.p99 << '\n';
    std::cout << "throughput Mpps: " << stats.throughput_mpps << '\n';

    rte_ring_free(ring);
    return 0;
#endif
}

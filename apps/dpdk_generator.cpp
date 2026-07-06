#include <cstddef>
#include <cstdint>
#include <iostream>

#include <tickforge/dpdk/market_transport.hpp>

namespace
{
[[maybe_unused]] [[nodiscard]] bool run_generator() noexcept
{
    tickforge::dpdk::MarketTransport transport{};
    if (!tickforge::dpdk::initialize_transport(transport, "tickforge_dpdk_ring", "tickforge_dpdk_pool"))
    {
        std::cerr << "Failed to initialize DPDK transport.\n";
        return false;
    }

    const dpdktrade::wire::MarketFrame frames[] = {
        tickforge::dpdk::make_market_frame(0U, 100U, 220U),
        tickforge::dpdk::make_market_frame(1U, 101U, 180U),
        tickforge::dpdk::make_market_frame(0U, 102U, 0U),
    };

    for (const auto& frame : frames)
    {
        if (!tickforge::dpdk::push_frame(transport, frame))
        {
            std::cerr << "Failed to enqueue market frame.\n";
            tickforge::dpdk::shutdown_transport(transport);
            return false;
        }
    }

    const std::size_t frame_count = sizeof(frames) / sizeof(frames[0]);
    std::cout << "DPDK generator queued " << frame_count << " MarketFrame objects.\n";
    tickforge::dpdk::shutdown_transport(transport);
    return true;
}
} // namespace

int main(int argc, char** argv)
{
#if !TICKFORGE_WITH_DPDK_HEADERS
    (void)argc;
    (void)argv;
    std::cerr << "DPDK headers are not available; DPDK generator is disabled.\n";
    return 0;
#else
    const char* eal_args[] = {argv[0], "-l", "0", "-n", "4"};
    const int eal_argc = static_cast<int>(sizeof(eal_args) / sizeof(eal_args[0]));
    if (rte_eal_init(eal_argc, const_cast<char**>(eal_args)) < 0)
    {
        std::cerr << "Failed to initialize DPDK EAL.\n";
        return 1;
    }

    (void)argc;
    return run_generator() ? 0 : 1;
#endif
}

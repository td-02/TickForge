#include <cstdint>
#include <iostream>

#include <dpdktrade/engine/dpdktrade_engine.hpp>
#include <tickforge/dpdk/market_transport.hpp>

namespace
{
[[maybe_unused]] [[nodiscard]] bool run_engine() noexcept
{
    tickforge::dpdk::MarketTransport transport{};
    if (!tickforge::dpdk::initialize_transport(transport, "tickforge_dpdk_ring", "tickforge_dpdk_pool"))
    {
        std::cerr << "Failed to initialize DPDK transport.\n";
        return false;
    }

    dpdktrade::engine::DpdkTradeEngine engine{dpdktrade::book::OrderBook{}, dpdktrade::risk::RiskGuard{{1000, 100000U}}};

    if (!tickforge::dpdk::push_frame(transport, tickforge::dpdk::make_market_frame(0U, 100U, 220U)))
    {
        std::cerr << "Failed to queue market frame.\n";
        tickforge::dpdk::shutdown_transport(transport);
        return false;
    }

    dpdktrade::wire::MarketFrame* frame = nullptr;
    if (!tickforge::dpdk::pop_frame(transport, frame))
    {
        std::cerr << "Failed to dequeue market frame.\n";
        tickforge::dpdk::shutdown_transport(transport);
        return false;
    }

    const auto order = engine.on_market(*frame);
    tickforge::dpdk::release_frame(transport, frame);
    tickforge::dpdk::shutdown_transport(transport);

    if (!order.has_value())
    {
        std::cerr << "Engine did not produce an order.\n";
        return false;
    }

    std::cout << "DPDK engine consumed one MarketFrame and produced one OrderFrame.\n";
    return true;
}
} // namespace

int main(int argc, char** argv)
{
#if !TICKFORGE_WITH_DPDK_HEADERS
    (void)argc;
    (void)argv;
    std::cerr << "DPDK headers are not available; DPDK engine is disabled.\n";
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
    return run_engine() ? 0 : 1;
#endif
}

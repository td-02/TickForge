#include <cstdint>
#include <iostream>

#include <tickforge/wire/frame.hpp>

#if defined(__has_include)
#    if __has_include(<rte_eal.h>) && __has_include(<rte_mbuf.h>) && __has_include(<rte_ethdev.h>)
#        include <rte_eal.h>
#        include <rte_mbuf.h>
#        include <rte_ethdev.h>
#        define TICKFORGE_HAS_DPDK 1
#    endif
#endif

#ifndef TICKFORGE_HAS_DPDK
#    define TICKFORGE_HAS_DPDK 0
#endif

namespace
{
[[nodiscard]] constexpr tickforge::wire::MarketFrame make_market_frame(std::uint64_t price, std::uint64_t quantity) noexcept
{
    tickforge::wire::MarketFrame frame{};
    frame.ethertype = tickforge::wire::ETHERTYPE_MARKET;
    frame.payload[0] = 0U;
    for (std::size_t index = 0; index < sizeof(std::uint64_t); ++index)
    {
        frame.payload[1 + index] = static_cast<std::uint8_t>((price >> (index * 8U)) & 0xFFU);
        frame.payload[9 + index] = static_cast<std::uint8_t>((quantity >> (index * 8U)) & 0xFFU);
    }
    return frame;
}
} // namespace

int main(int argc, char** argv)
{
#if !TICKFORGE_HAS_DPDK
    (void)argc;
    (void)argv;
    const auto frame = make_market_frame(100U, 10U);
    std::cout << "DPDK headers are not available; AF_PACKET generator is disabled.\n";
    std::cout << "Example MarketFrame ethertype: " << frame.ethertype << '\n';
    return 0;
#else
    const char* eal_args[] = {argv[0], "-l", "0", "-n", "4"};
    const int eal_result = rte_eal_init(4, const_cast<char**>(eal_args));
    if (eal_result < 0)
    {
        std::cerr << "Failed to initialize DPDK EAL.\n";
        return 1;
    }

    const auto frame = make_market_frame(100U, 10U);
    std::cout << "TickForge AF_PACKET generator ready.\n";
    std::cout << "MarketFrame ethertype: " << frame.ethertype << '\n';
    std::cout << "Transmit on the veth pair configured for the AF_PACKET PMD.\n";

    (void)argc;
    return 0;
#endif
}

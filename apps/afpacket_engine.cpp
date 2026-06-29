#include <cstdint>
#include <iostream>

#if defined(__has_include)
#    if __has_include(<rte_eal.h>) && __has_include(<rte_ethdev.h>) && __has_include(<rte_malloc.h>)
#        include <rte_eal.h>
#        include <rte_ethdev.h>
#        include <rte_malloc.h>
#        define TICKFORGE_HAS_DPDK 1
#    endif
#endif

#ifndef TICKFORGE_HAS_DPDK
#    define TICKFORGE_HAS_DPDK 0
#endif

int main(int argc, char** argv)
{
#if !TICKFORGE_HAS_DPDK
    (void)argc;
    (void)argv;
    std::cerr << "DPDK headers are not available; AF_PACKET engine is disabled.\n";
    return 0;
#else
    const char* eal_args[] = {argv[0], "-l", "0", "-n", "4"};
    const int eal_result = rte_eal_init(4, const_cast<char**>(eal_args));
    if (eal_result < 0)
    {
        std::cerr << "Failed to initialize DPDK EAL.\n";
        return 1;
    }

    std::cout << "TickForge AF_PACKET engine initialized.\n";
    std::cout << "Attach a Linux veth pair through the AF_PACKET PMD and forward MarketFrame traffic here.\n";
    std::cout << "OrderFrame responses should be emitted on the paired TX path.\n";

    (void)argc;
    return 0;
#endif
}

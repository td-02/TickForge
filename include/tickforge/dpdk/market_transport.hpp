#pragma once

#include <cstddef>
#include <cstdint>

#include <dpdktrade/wire/frame.hpp>

#if defined(__has_include)
#    if __has_include(<rte_eal.h>) && __has_include(<rte_lcore.h>) && __has_include(<rte_mempool.h>) && __has_include(<rte_ring.h>)
#        include <rte_eal.h>
#        include <rte_lcore.h>
#        include <rte_mempool.h>
#        include <rte_ring.h>
#        define TICKFORGE_WITH_DPDK_HEADERS 1
#    endif
#endif

#ifndef TICKFORGE_WITH_DPDK_HEADERS
#    define TICKFORGE_WITH_DPDK_HEADERS 0
#endif

namespace tickforge::dpdk
{
struct MarketTransport final
{
#if TICKFORGE_WITH_DPDK_HEADERS
    rte_ring* ring = nullptr;
    rte_mempool* pool = nullptr;
#else
    void* ring = nullptr;
    void* pool = nullptr;
#endif

    [[nodiscard]] constexpr bool ready() const noexcept
    {
        return ring != nullptr && pool != nullptr;
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

#if TICKFORGE_WITH_DPDK_HEADERS
[[nodiscard]] inline bool initialize_transport(MarketTransport& transport, const char* ring_name, const char* pool_name, unsigned capacity = 4096U) noexcept
{
    rte_mempool* pool = rte_mempool_create(
        pool_name,
        capacity,
        sizeof(dpdktrade::wire::MarketFrame),
        0U,
        0U,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        rte_socket_id(),
        0U);
    if (pool == nullptr)
    {
        return false;
    }

    rte_ring* ring = rte_ring_create(ring_name, capacity, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    if (ring == nullptr)
    {
        rte_mempool_free(pool);
        return false;
    }

    transport.ring = ring;
    transport.pool = pool;
    return true;
}

inline void shutdown_transport(MarketTransport& transport) noexcept
{
    if (transport.ring != nullptr)
    {
        rte_ring_free(transport.ring);
        transport.ring = nullptr;
    }

    if (transport.pool != nullptr)
    {
        rte_mempool_free(transport.pool);
        transport.pool = nullptr;
    }
}

[[nodiscard]] inline dpdktrade::wire::MarketFrame* acquire_frame(MarketTransport& transport) noexcept
{
    if (!transport.ready())
    {
        return nullptr;
    }

    void* object = nullptr;
    if (rte_mempool_get(transport.pool, &object) != 0)
    {
        return nullptr;
    }

    return static_cast<dpdktrade::wire::MarketFrame*>(object);
}

inline void release_frame(MarketTransport& transport, dpdktrade::wire::MarketFrame* frame) noexcept
{
    if (transport.pool != nullptr && frame != nullptr)
    {
        rte_mempool_put(transport.pool, frame);
    }
}

[[nodiscard]] inline bool push_frame(MarketTransport& transport, const dpdktrade::wire::MarketFrame& frame) noexcept
{
    dpdktrade::wire::MarketFrame* stored = acquire_frame(transport);
    if (stored == nullptr)
    {
        return false;
    }

    *stored = frame;
    if (rte_ring_enqueue(transport.ring, stored) != 0)
    {
        release_frame(transport, stored);
        return false;
    }

    return true;
}

[[nodiscard]] inline bool pop_frame(MarketTransport& transport, dpdktrade::wire::MarketFrame*& frame) noexcept
{
    frame = nullptr;
    if (!transport.ready())
    {
        return false;
    }

    void* object = nullptr;
    if (rte_ring_dequeue(transport.ring, &object) != 0)
    {
        return false;
    }

    frame = static_cast<dpdktrade::wire::MarketFrame*>(object);
    return true;
}
#endif
} // namespace tickforge::dpdk

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <tickforge/dpdk/market_transport.hpp>

int main()
{
    using namespace tickforge::dpdk;

    MarketTransport transport{};

#if TICKFORGE_WITH_DPDK_HEADERS
    assert(initialize_transport(transport, "tickforge_test_ring", "tickforge_test_pool", 8U));
    assert(transport.ready());

    dpdktrade::wire::MarketFrame frame{};
    frame.ethertype = dpdktrade::wire::ETHERTYPE_MARKET;
    frame.payload[0] = 1U;
    frame.payload[1] = 0x34U;
    frame.payload[2] = 0x12U;
    frame.payload[9] = 0x78U;
    frame.payload[10] = 0x56U;

    assert(push_frame(transport, frame));

    dpdktrade::wire::MarketFrame* received = nullptr;
    assert(pop_frame(transport, received));
    assert(received != nullptr);
    assert(received->ethertype == frame.ethertype);
    assert(received->payload[0] == frame.payload[0]);
    assert(received->payload[1] == frame.payload[1]);
    assert(received->payload[2] == frame.payload[2]);
    assert(received->payload[9] == frame.payload[9]);
    assert(received->payload[10] == frame.payload[10]);
    release_frame(transport, received);

    dpdktrade::wire::MarketFrame malformed{};
    malformed.ethertype = 0xFFFFU;
    malformed.payload[0] = 0x7FU;
    malformed.payload[1] = 0xAAU;
    malformed.payload[9] = 0xBBU;
    assert(push_frame(transport, malformed));

    received = nullptr;
    assert(pop_frame(transport, received));
    assert(received != nullptr);
    assert(received->ethertype == malformed.ethertype);
    assert(received->payload[0] == malformed.payload[0]);
    assert(received->payload[1] == malformed.payload[1]);
    assert(received->payload[9] == malformed.payload[9]);
    release_frame(transport, received);

    assert(!pop_frame(transport, received));

    for (unsigned index = 0; index < 8U; ++index)
    {
        dpdktrade::wire::MarketFrame full_frame{};
        full_frame.ethertype = dpdktrade::wire::ETHERTYPE_MARKET;
        full_frame.payload[0] = static_cast<std::uint8_t>(index & 1U);
        assert(push_frame(transport, full_frame));
    }

    dpdktrade::wire::MarketFrame overflow{};
    overflow.ethertype = dpdktrade::wire::ETHERTYPE_MARKET;
    assert(!push_frame(transport, overflow));

    for (unsigned index = 0; index < 8U; ++index)
    {
        received = nullptr;
        assert(pop_frame(transport, received));
        assert(received != nullptr);
        release_frame(transport, received);
    }

    assert(!pop_frame(transport, received));
    shutdown_transport(transport);
#else
    assert(!transport.ready());
    dpdktrade::wire::MarketFrame frame{};
    dpdktrade::wire::MarketFrame* received = nullptr;
    assert(!initialize_transport(transport, "tickforge_test_ring", "tickforge_test_pool"));
    assert(acquire_frame(transport) == nullptr);
    assert(!push_frame(transport, frame));
    assert(!pop_frame(transport, received));
#endif

    return 0;
}

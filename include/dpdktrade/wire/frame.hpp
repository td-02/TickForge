#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace dpdktrade::wire
{
inline constexpr std::uint16_t ETHERTYPE_MARKET = 0xF001U;
inline constexpr std::uint16_t ETHERTYPE_ORDER = 0xF002U;

namespace detail
{
#if defined(_MSC_VER)
#    pragma pack(push, 1)
#endif

#if defined(__GNUC__) || defined(__clang__)
struct __attribute__((packed)) MarketFrame final
#elif defined(_MSC_VER)
struct MarketFrame final
#else
struct MarketFrame final
#endif
{
    std::uint16_t ethertype;
    std::uint8_t payload[60];
};

#if defined(__GNUC__) || defined(__clang__)
struct __attribute__((packed)) OrderFrame final
#elif defined(_MSC_VER)
struct OrderFrame final
#else
struct OrderFrame final
#endif
{
    std::uint16_t ethertype;
    std::uint8_t payload[60];
};

#if defined(_MSC_VER)
#    pragma pack(pop)
#endif

static_assert(sizeof(MarketFrame) == 62, "MarketFrame must remain exactly 62 bytes.");
static_assert(sizeof(OrderFrame) == 62, "OrderFrame must remain exactly 62 bytes.");
static_assert(std::is_trivially_copyable_v<MarketFrame>, "MarketFrame must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<OrderFrame>, "OrderFrame must be trivially copyable.");
} // namespace detail

using MarketFrame = detail::MarketFrame;
using OrderFrame = detail::OrderFrame;

static_assert(sizeof(MarketFrame) == 62, "MarketFrame size validation failed.");
static_assert(sizeof(OrderFrame) == 62, "OrderFrame size validation failed.");

[[nodiscard]] inline const MarketFrame* decode_market(std::span<const std::byte, sizeof(MarketFrame)> bytes) noexcept
{
    // Zero-copy decode:
    // The protocol is intentionally represented as a packed POD so the caller can
    // reinterpret an aligned byte buffer as a frame without allocating or copying.
    //
    // This function returns a pointer rather than materializing a new object. That
    // keeps the hot path deterministic and avoids any hidden allocation or exception
    // machinery. The caller owns the lifetime of the backing storage.
    return reinterpret_cast<const MarketFrame*>(bytes.data());
}

[[nodiscard]] inline const MarketFrame* decode_market(const std::byte* bytes) noexcept
{
    return reinterpret_cast<const MarketFrame*>(bytes);
}

[[nodiscard]] constexpr std::array<std::byte, sizeof(OrderFrame)> encode_order(const OrderFrame& frame) noexcept
{
    // Encoding is a straight byte-for-byte projection of the packed wire image.
    // Returning a fixed-size array preserves deterministic storage requirements and
    // keeps the function free of heap allocation, exceptions, and dynamic growth.
    return std::bit_cast<std::array<std::byte, sizeof(OrderFrame)>>(frame);
}
} // namespace dpdktrade::wire

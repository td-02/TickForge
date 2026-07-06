#pragma once

#include <cstdint>

namespace dpdktrade::risk
{
class RiskGuard final
{
public:
    struct Limits final
    {
        std::int64_t max_position = 0;
        std::uint64_t max_notional = 0;
    };

    constexpr explicit RiskGuard(Limits limits) noexcept
        : limits_{limits}
    {
    }

    // Constant-time pre-trade check:
    // The guard rejects by default and only updates internal state after all
    // limits are satisfied. This fail-closed structure keeps the hot path
    // deterministic and avoids partial updates on rejection.
    [[nodiscard]] constexpr bool check_and_update(std::int64_t signed_quantity, std::uint64_t price) noexcept
    {
        const std::int64_t next_position = position_ + signed_quantity;

        if (next_position > limits_.max_position || next_position < -limits_.max_position)
        {
            return false;
        }

        const std::uint64_t increment = abs_signed_quantity(signed_quantity) * price;
        const std::uint64_t next_notional = notional_ + increment;

        if (next_notional > limits_.max_notional)
        {
            return false;
        }

        position_ = next_position;
        notional_ = next_notional;
        return true;
    }

    constexpr void reset() noexcept
    {
        position_ = 0;
        notional_ = 0;
    }

    [[nodiscard]] constexpr std::int64_t position() const noexcept
    {
        return position_;
    }

    [[nodiscard]] constexpr std::uint64_t notional() const noexcept
    {
        return notional_;
    }

private:
    Limits limits_{};
    std::int64_t position_ = 0;
    std::uint64_t notional_ = 0;

    [[nodiscard]] static constexpr std::uint64_t abs_signed_quantity(std::int64_t quantity) noexcept
    {
        return quantity < 0 ? static_cast<std::uint64_t>(-quantity) : static_cast<std::uint64_t>(quantity);
    }
};
} // namespace dpdktrade::risk

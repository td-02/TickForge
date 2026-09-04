#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dpdktrade::book
{
class OrderBook final
{
public:
    static constexpr std::size_t depth = 10;

    struct Level final
    {
        std::uint64_t price = 0;
        std::uint64_t quantity = 0;
    };
    static_assert(sizeof(Level) == 16, "Level should remain a compact pair of uint64_t values");

    enum class Side : std::uint8_t
    {
        Bid = 0,
        Ask = 1,
    };

    constexpr OrderBook() noexcept = default;

    // Apply a price/quantity update to one side of the book.
    // The book remains fixed-size so the hot path stays predictable and
    // avoids any hidden allocation or map lookup cost.
    constexpr void apply(Side side, std::uint64_t price, std::uint64_t quantity) noexcept
    {
        if (side == Side::Bid)
        {
            apply_side<true>(price, quantity);
            return;
        }

        apply_side<false>(price, quantity);
    }

    [[nodiscard]] constexpr std::uint64_t bid_pressure() const noexcept
    {
        return bid_pressure_;
    }

    [[nodiscard]] constexpr std::uint64_t ask_pressure() const noexcept
    {
        return ask_pressure_;
    }

    [[nodiscard]] constexpr Level best_bid() const noexcept
    {
        return bid_count_ == 0 ? Level{} : bid_levels_[0];
    }

    [[nodiscard]] constexpr Level best_ask() const noexcept
    {
        return ask_count_ == 0 ? Level{} : ask_levels_[0];
    }

private:
    alignas(64) std::array<Level, depth> bid_levels_{};
    alignas(64) std::array<Level, depth> ask_levels_{};
    std::size_t bid_count_ = 0;
    std::size_t ask_count_ = 0;
    std::uint64_t bid_pressure_ = 0;
    std::uint64_t ask_pressure_ = 0;

    template <bool IsBid>
    constexpr void apply_side(std::uint64_t price, std::uint64_t quantity) noexcept
    {
        auto& levels = IsBid ? bid_levels_ : ask_levels_;
        std::size_t& count = IsBid ? bid_count_ : ask_count_;
        std::uint64_t& pressure = IsBid ? bid_pressure_ : ask_pressure_;

        for (std::size_t index = 0; index < count; ++index)
        {
            if (levels[index].price != price)
            {
                continue;
            }

            if (quantity == 0)
            {
                pressure -= levels[index].quantity;
                for (std::size_t current = index; current + 1 < count; ++current)
                {
                    levels[current] = levels[current + 1];
                }
                levels[count - 1] = Level{};
                --count;
                return;
            }

            pressure += quantity - levels[index].quantity;
            levels[index].quantity = quantity;
            return;
        }

        if (quantity == 0)
        {
            return;
        }

        if (count < depth)
        {
            std::size_t insert_at = count;
            for (std::size_t index = 0; index < count; ++index)
            {
                if constexpr (IsBid)
                {
                    if (price > levels[index].price)
                    {
                        insert_at = index;
                        break;
                    }
                }
                else if (price < levels[index].price)
                {
                    insert_at = index;
                    break;
                }
            }

            for (std::size_t current = count; current > insert_at; --current)
            {
                levels[current] = levels[current - 1];
            }

            levels[insert_at] = Level{price, quantity};
            pressure += quantity;
            ++count;
            return;
        }

        pressure -= levels[depth - 1].quantity;
        std::size_t insert_at = depth;
        for (std::size_t index = 0; index < depth; ++index)
        {
            if constexpr (IsBid)
            {
                if (price > levels[index].price)
                {
                    insert_at = index;
                    break;
                }
            }
            else
            {
                if (price < levels[index].price)
                {
                    insert_at = index;
                    break;
                }
            }
        }

        if (insert_at == depth)
        {
            pressure += quantity;
            levels[depth - 1] = Level{price, quantity};
            return;
        }

        for (std::size_t current = depth - 1; current > insert_at; --current)
        {
            levels[current] = levels[current - 1];
        }

        if constexpr (IsBid)
        {
            levels[insert_at] = Level{price, quantity};
            pressure += quantity;
            return;
        }

        levels[insert_at] = Level{price, quantity};
        pressure += quantity;
    }
};
} // namespace dpdktrade::book

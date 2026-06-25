#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace tickforge::book
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
        auto& levels = (side == Side::Bid) ? bid_levels_ : ask_levels_;

        for (std::size_t index = 0; index < depth; ++index)
        {
            if (levels[index].price == price || levels[index].quantity == 0)
            {
                levels[index].price = price;
                levels[index].quantity = quantity;
                return;
            }
        }

        // When the side is full, replace the last slot deterministically.
        levels[depth - 1].price = price;
        levels[depth - 1].quantity = quantity;
    }

    [[nodiscard]] constexpr std::uint64_t bid_pressure() const noexcept
    {
        return side_pressure(bid_levels_);
    }

    [[nodiscard]] constexpr std::uint64_t ask_pressure() const noexcept
    {
        return side_pressure(ask_levels_);
    }

    [[nodiscard]] constexpr Level best_bid() const noexcept
    {
        return best_level(bid_levels_, true);
    }

    [[nodiscard]] constexpr Level best_ask() const noexcept
    {
        return best_level(ask_levels_, false);
    }

private:
    std::array<Level, depth> bid_levels_{};
    std::array<Level, depth> ask_levels_{};

    [[nodiscard]] static constexpr std::uint64_t side_pressure(const std::array<Level, depth>& levels) noexcept
    {
        std::uint64_t total = 0;
        for (const auto& level : levels)
        {
            total += level.quantity;
        }
        return total;
    }

    [[nodiscard]] static constexpr Level best_level(const std::array<Level, depth>& levels, bool highest_price) noexcept
    {
        Level best{};
        bool found = false;

        for (const auto& level : levels)
        {
            if (level.quantity == 0)
            {
                continue;
            }

            if (!found)
            {
                best = level;
                found = true;
                continue;
            }

            if (highest_price)
            {
                if (level.price > best.price)
                {
                    best = level;
                }
            }
            else
            {
                if (level.price < best.price)
                {
                    best = level;
                }
            }
        }

        return best;
    }
};
} // namespace tickforge::book

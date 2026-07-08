#include <cassert>
#include <cstdint>

#include <dpdktrade/book/order_book.hpp>
#include <dpdktrade/strategy/imbalance.hpp>
#include <dpdktrade/strategy/mean_reversion.hpp>
#include <dpdktrade/strategy/momentum.hpp>

namespace
{
[[nodiscard]] constexpr dpdktrade::book::OrderBook make_book(std::uint64_t bid_price,
                                                             std::uint64_t bid_quantity,
                                                             std::uint64_t ask_price,
                                                             std::uint64_t ask_quantity) noexcept
{
    dpdktrade::book::OrderBook order_book{};
    order_book.apply(dpdktrade::book::OrderBook::Side::Bid, bid_price, bid_quantity);
    order_book.apply(dpdktrade::book::OrderBook::Side::Ask, ask_price, ask_quantity);
    return order_book;
}
} // namespace

int main()
{
    using namespace dpdktrade;

    {
        strategy::ImbalanceStrategy strategy{};
        assert(strategy.evaluate(make_book(100U, 220U, 101U, 100U)) == strategy::Signal::BUY);
        assert(strategy.evaluate(make_book(100U, 100U, 101U, 220U)) == strategy::Signal::SELL);
        assert(strategy.evaluate(make_book(100U, 110U, 101U, 110U)) == strategy::Signal::NO_SIGNAL);
    }

    {
        strategy::MomentumStrategy strategy{};
        for (std::uint64_t index = 0; index < strategy::MomentumStrategy::history_size; ++index)
        {
            (void)index;
            const auto neutral_book = make_book(100U, 10U, 101U, 10U);
            assert(strategy.evaluate(neutral_book) == strategy::Signal::NO_SIGNAL);
        }
        assert(strategy.evaluate(make_book(120U, 10U, 121U, 10U)) == strategy::Signal::BUY);
    }

    {
        strategy::MomentumStrategy strategy{};
        for (std::uint64_t index = 0; index < strategy::MomentumStrategy::history_size; ++index)
        {
            (void)index;
            const auto neutral_book = make_book(100U, 10U, 101U, 10U);
            assert(strategy.evaluate(neutral_book) == strategy::Signal::NO_SIGNAL);
        }
        assert(strategy.evaluate(make_book(96U, 10U, 97U, 10U)) == strategy::Signal::SELL);
    }

    {
        strategy::MeanReversionStrategy strategy{};
        for (std::uint64_t index = 0; index < strategy::MeanReversionStrategy::history_size; ++index)
        {
            (void)index;
            const auto neutral_book = make_book(100U, 10U, 102U, 10U);
            assert(strategy.evaluate(neutral_book) == strategy::Signal::NO_SIGNAL);
        }
        assert(strategy.evaluate(make_book(96U, 10U, 98U, 10U)) == strategy::Signal::BUY);
    }

    {
        strategy::MeanReversionStrategy strategy{};
        for (std::uint64_t index = 0; index < strategy::MeanReversionStrategy::history_size; ++index)
        {
            (void)index;
            const auto neutral_book = make_book(100U, 10U, 102U, 10U);
            assert(strategy.evaluate(neutral_book) == strategy::Signal::NO_SIGNAL);
        }
        assert(strategy.evaluate(make_book(104U, 10U, 106U, 10U)) == strategy::Signal::SELL);
    }

    return 0;
}

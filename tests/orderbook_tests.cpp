// Dependency-free unit tests. Run with: ctest --test-dir build --output-on-failure

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "orderbook/orderbook.hpp"

using namespace ob;

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++g_failures;                                                             \
            std::cerr << __FILE__ << ":" << __LINE__ << "  CHECK failed: " #cond "\n"; \
        }                                                                             \
    } while (0)

OrderPointer make(OrderId id, OrderType type, Side side, Price price, Quantity qty) {
    return std::make_shared<Order>(id, type, side, price, qty);
}
OrderPointer gtc(OrderId id, Side side, Price price, Quantity qty) {
    return make(id, OrderType::GoodTillCancel, side, price, qty);
}

// ---------------------------------------------------------------------------

void restingOrdersDoNotMatch() {
    OrderBook book;
    auto t1 = book.addOrder(gtc(1, Side::Buy, 100, 10));
    auto t2 = book.addOrder(gtc(2, Side::Sell, 101, 5));

    CHECK(t1 && t1->empty());
    CHECK(t2 && t2->empty());
    CHECK(book.size() == 2);

    const auto infos = book.getOrderLevelInfos();
    CHECK(infos.getBids().size() == 1);
    CHECK(infos.getAsks().size() == 1);
    CHECK(infos.getBids()[0].price == 100 && infos.getBids()[0].quantity == 10);
    CHECK(infos.getAsks()[0].price == 101 && infos.getAsks()[0].quantity == 5);
}

void levelsAreSortedBestFirst() {
    OrderBook book;
    book.addOrder(gtc(1, Side::Buy, 98, 1));
    book.addOrder(gtc(2, Side::Buy, 100, 1));
    book.addOrder(gtc(3, Side::Buy, 99, 1));
    book.addOrder(gtc(4, Side::Sell, 105, 1));
    book.addOrder(gtc(5, Side::Sell, 102, 1));
    book.addOrder(gtc(6, Side::Sell, 103, 1));

    const auto infos = book.getOrderLevelInfos();
    CHECK(infos.getBids().size() == 3);
    CHECK(infos.getBids()[0].price == 100);
    CHECK(infos.getBids()[1].price == 99);
    CHECK(infos.getBids()[2].price == 98);
    CHECK(infos.getAsks().size() == 3);
    CHECK(infos.getAsks()[0].price == 102);
    CHECK(infos.getAsks()[1].price == 103);
    CHECK(infos.getAsks()[2].price == 105);
}

void timePriorityWithinLevel() {
    OrderBook book;
    book.addOrder(gtc(1, Side::Buy, 100, 5));
    book.addOrder(gtc(2, Side::Buy, 100, 5));
    auto trades = book.addOrder(gtc(3, Side::Sell, 100, 7));

    CHECK(trades && trades->size() == 2);
    CHECK((*trades)[0].getBidTrade().orderId == 1);
    CHECK((*trades)[0].getBidTrade().quantity == 5);
    CHECK((*trades)[1].getBidTrade().orderId == 2);
    CHECK((*trades)[1].getBidTrade().quantity == 2);
    CHECK((*trades)[1].getAskTrade().orderId == 3);

    CHECK(book.size() == 1);
    const auto infos = book.getOrderLevelInfos();
    CHECK(infos.getBids().size() == 1 && infos.getBids()[0].quantity == 3);
    CHECK(infos.getAsks().empty());
}

void betterPriceMatchesFirst() {
    OrderBook book;
    book.addOrder(gtc(1, Side::Sell, 101, 5));
    book.addOrder(gtc(2, Side::Sell, 100, 5));
    auto trades = book.addOrder(gtc(3, Side::Buy, 101, 5));

    CHECK(trades && trades->size() == 1);
    CHECK((*trades)[0].getAskTrade().orderId == 2);
    CHECK((*trades)[0].getAskTrade().price == 100);  // each side keeps its own limit
    CHECK((*trades)[0].getBidTrade().price == 101);

    const auto infos = book.getOrderLevelInfos();
    CHECK(infos.getAsks().size() == 1 && infos.getAsks()[0].price == 101);
}

void partialFillRests() {
    OrderBook book;
    book.addOrder(gtc(1, Side::Buy, 100, 10));
    auto trades = book.addOrder(gtc(2, Side::Sell, 100, 4));

    CHECK(trades && trades->size() == 1);
    CHECK((*trades)[0].getBidTrade().quantity == 4);
    CHECK(book.size() == 1);
    CHECK(book.getOrderLevelInfos().getBids()[0].quantity == 6);
}

void fillAndKill() {
    {   // nothing to match against -> rejected
        OrderBook book;
        auto r = book.addOrder(make(1, OrderType::FillAndKill, Side::Buy, 100, 5));
        CHECK(!r.has_value());
        CHECK(book.size() == 0);
    }
    {   // partially filled -> remainder cancelled, nothing rests
        OrderBook book;
        book.addOrder(gtc(1, Side::Sell, 100, 5));
        auto r = book.addOrder(make(2, OrderType::FillAndKill, Side::Buy, 100, 8));
        CHECK(r && r->size() == 1);
        CHECK((*r)[0].getBidTrade().quantity == 5);
        CHECK(book.size() == 0);
        CHECK(book.getOrderLevelInfos().getBids().empty());
    }
}

void fillOrKill() {
    OrderBook book;
    book.addOrder(gtc(1, Side::Sell, 100, 5));

    auto tooBig = book.addOrder(make(2, OrderType::FillOrKill, Side::Buy, 100, 8));
    CHECK(!tooBig.has_value());
    CHECK(book.size() == 1);  // book untouched
    CHECK(book.getOrderLevelInfos().getAsks()[0].quantity == 5);

    auto exact = book.addOrder(make(3, OrderType::FillOrKill, Side::Buy, 100, 5));
    CHECK(exact && exact->size() == 1);
    CHECK(book.size() == 0);
}

void fillOrKillAcrossLevelsRespectsLimit() {
    OrderBook book;
    book.addOrder(gtc(1, Side::Sell, 100, 5));
    book.addOrder(gtc(2, Side::Sell, 101, 5));

    // Only 5 shares are available at <= 100.
    auto blocked = book.addOrder(make(3, OrderType::FillOrKill, Side::Buy, 100, 10));
    CHECK(!blocked.has_value());
    CHECK(book.size() == 2);

    auto ok = book.addOrder(make(4, OrderType::FillOrKill, Side::Buy, 101, 10));
    CHECK(ok && ok->size() == 2);
    CHECK(book.size() == 0);
}

void marketOrders() {
    {   // empty book: accepted, but no trades and nothing rests
        OrderBook book;
        auto r = book.addOrder(make(1, OrderType::Market, Side::Buy, kNoPrice, 5));
        CHECK(r && r->empty());
        CHECK(book.size() == 0);
    }
    {   // market buy sweeps asks, best price first
        OrderBook book;
        book.addOrder(gtc(1, Side::Sell, 100, 5));
        book.addOrder(gtc(2, Side::Sell, 101, 5));
        auto r = book.addOrder(make(3, OrderType::Market, Side::Buy, kNoPrice, 7));

        CHECK(r && r->size() == 2);
        CHECK((*r)[0].getBidTrade().orderId == 3);
        CHECK((*r)[0].getAskTrade().orderId == 1);
        CHECK((*r)[0].getAskTrade().price == 100 && (*r)[0].getBidTrade().price == 100);
        CHECK((*r)[0].getBidTrade().quantity == 5);
        CHECK((*r)[1].getAskTrade().orderId == 2);
        CHECK((*r)[1].getBidTrade().price == 101);
        CHECK((*r)[1].getBidTrade().quantity == 2);

        const auto infos = book.getOrderLevelInfos();
        CHECK(infos.getAsks().size() == 1);
        CHECK(infos.getAsks()[0].price == 101 && infos.getAsks()[0].quantity == 3);
    }
    {   // market sell: the incoming order must be reported as the ask side
        OrderBook book;
        book.addOrder(gtc(1, Side::Buy, 100, 5));
        auto r = book.addOrder(make(2, OrderType::Market, Side::Sell, kNoPrice, 3));

        CHECK(r && r->size() == 1);
        CHECK((*r)[0].getBidTrade().orderId == 1);
        CHECK((*r)[0].getAskTrade().orderId == 2);
        CHECK((*r)[0].getBidTrade().price == 100);
        CHECK(book.getOrderLevelInfos().getBids()[0].quantity == 2);
    }
    {   // unfilled remainder of a market order is discarded
        OrderBook book;
        book.addOrder(gtc(1, Side::Sell, 100, 2));
        auto r = book.addOrder(make(2, OrderType::Market, Side::Buy, kNoPrice, 10));
        CHECK(r && r->size() == 1);
        CHECK(book.size() == 0);
    }
}

void cancelFromMiddleOfLevel() {
    OrderBook book;
    book.addOrder(gtc(1, Side::Buy, 100, 5));
    book.addOrder(gtc(2, Side::Buy, 100, 5));
    book.addOrder(gtc(3, Side::Buy, 100, 5));

    book.cancelOrder(2);
    CHECK(book.size() == 2);

    auto trades = book.addOrder(gtc(4, Side::Sell, 100, 10));
    CHECK(trades && trades->size() == 2);
    CHECK((*trades)[0].getBidTrade().orderId == 1);
    CHECK((*trades)[1].getBidTrade().orderId == 3);
    CHECK(book.size() == 0);
}

void cancelAfterEarlierOrderWasFilled() {
    // Regression test: positions inside a level shift once the front order is
    // filled, so cancellation must not rely on a stored numeric index.
    OrderBook book;
    book.addOrder(gtc(1, Side::Buy, 100, 5));
    book.addOrder(gtc(2, Side::Buy, 100, 5));
    book.addOrder(gtc(3, Side::Buy, 100, 5));

    book.addOrder(gtc(4, Side::Sell, 100, 5));  // fills order 1
    book.cancelOrder(3);

    CHECK(book.size() == 1);
    CHECK(book.getOrderLevelInfos().getBids()[0].quantity == 5);

    auto trades = book.addOrder(gtc(5, Side::Sell, 100, 5));
    CHECK(trades && trades->size() == 1);
    CHECK((*trades)[0].getBidTrade().orderId == 2);
    CHECK(book.size() == 0);
}

void cancelRemovesEmptyLevelAndIgnoresUnknownIds() {
    OrderBook book;
    book.addOrder(gtc(1, Side::Sell, 100, 5));
    book.cancelOrder(999);  // unknown: no-op
    CHECK(book.size() == 1);

    book.cancelOrder(1);
    CHECK(book.size() == 0);
    CHECK(book.getOrderLevelInfos().getAsks().empty());

    book.cancelOrder(1);  // already gone: no-op
    CHECK(book.size() == 0);
}

void modifyLosesTimePriority() {
    OrderBook book;
    book.addOrder(gtc(1, Side::Buy, 100, 5));
    book.addOrder(gtc(2, Side::Buy, 100, 5));

    auto m = book.modifyOrder(OrderModify{1, Side::Buy, 100, 5});
    CHECK(m && m->empty());

    auto trades = book.addOrder(gtc(3, Side::Sell, 100, 5));
    CHECK(trades && trades->size() == 1);
    CHECK((*trades)[0].getBidTrade().orderId == 2);  // order 1 now queues behind 2
}

void modifyCanCrossTheSpread() {
    OrderBook book;
    book.addOrder(gtc(9, Side::Sell, 101, 5));
    book.addOrder(gtc(1, Side::Buy, 100, 5));

    auto trades = book.modifyOrder(OrderModify{1, Side::Buy, 101, 5});
    CHECK(trades && trades->size() == 1);
    CHECK((*trades)[0].getAskTrade().orderId == 9);
    CHECK(book.size() == 0);

    CHECK(!book.modifyOrder(OrderModify{12345, Side::Buy, 100, 1}).has_value());
}

void invalidOrdersAreRejected() {
    OrderBook book;
    CHECK(book.addOrder(gtc(1, Side::Buy, 100, 5)).has_value());

    CHECK(!book.addOrder(gtc(1, Side::Sell, 200, 5)).has_value());      // duplicate id
    CHECK(!book.addOrder(gtc(2, Side::Buy, std::nanf(""), 5)).has_value());  // NaN price
    CHECK(!book.addOrder(gtc(3, Side::Buy, 100, 0)).has_value());        // zero quantity
    CHECK(!book.addOrder(nullptr).has_value());                          // null order
    CHECK(book.size() == 1);
}

void overfillThrows() {
    Order order{1, OrderType::GoodTillCancel, Side::Buy, 100, 5};

    bool threw = false;
    try {
        order.fill(6);
    } catch (const std::logic_error&) {
        threw = true;
    }
    CHECK(threw);
    CHECK(order.getRemainingQuantity() == 5);  // unchanged after the failed fill

    order.fill(5);
    CHECK(order.isFilled());
    CHECK(order.getFilledQuantity() == 5);
}

}  // namespace

int main() {
    restingOrdersDoNotMatch();
    levelsAreSortedBestFirst();
    timePriorityWithinLevel();
    betterPriceMatchesFirst();
    partialFillRests();
    fillAndKill();
    fillOrKill();
    fillOrKillAcrossLevelsRespectsLimit();
    marketOrders();
    cancelFromMiddleOfLevel();
    cancelAfterEarlierOrderWasFilled();
    cancelRemovesEmptyLevelAndIgnoresUnknownIds();
    modifyLosesTimePriority();
    modifyCanCrossTheSpread();
    invalidOrdersAreRejected();
    overfillThrows();

    if (g_failures == 0) {
        std::cout << "All tests passed\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
}

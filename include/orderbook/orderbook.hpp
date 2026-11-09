#pragma once

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <map>
#include <optional>
#include <unordered_map>

#include "orderbook/level_info.hpp"
#include "orderbook/order.hpp"
#include "orderbook/order_modify.hpp"
#include "orderbook/trade.hpp"

namespace ob {

// In-memory limit order book with price-time (FIFO) priority.
//
// Supported order types: GoodTillCancel, FillAndKill, FillOrKill, Market.
// Not thread-safe: use one OrderBook per thread or add external locking.
class OrderBook {
public:
    // Adds an order and runs the matching engine.
    //  - returns std::nullopt if the order is rejected (duplicate id, invalid
    //    price/quantity, FillAndKill that cannot match, FillOrKill that cannot
    //    be filled completely)
    //  - otherwise returns the trades generated (possibly an empty vector)
    std::optional<Trades> addOrder(OrderPointer order);

    // Removes a resting order. Unknown ids are ignored.
    void cancelOrder(OrderId orderId);

    // Cancel + re-add with the same order type. The order loses time priority.
    std::optional<Trades> modifyOrder(const OrderModify& modify);

    // Number of resting orders.
    [[nodiscard]] std::size_t size() const noexcept { return orders_.size(); }

    [[nodiscard]] OrderbookLevelInfos getOrderLevelInfos() const;

    void print() const;
    void print(std::ostream& os) const;

private:
    struct OrderEntry {
        OrderPointer           order;
        OrderPointers::iterator location;  // position inside its price level
    };

    // Bids: highest price first. Asks: lowest price first.
    // begin() is therefore always the best price on either side.
    using Bids = std::map<Price, OrderPointers, std::greater<Price>>;
    using Asks = std::map<Price, OrderPointers, std::less<Price>>;

    [[nodiscard]] bool canMatch(Side side, Price price) const;
    [[nodiscard]] bool canFullyMatch(const Order& order) const;

    Trades matchOrders();

    template <class Book>
    Trades matchMarketOrder(Book& oppositeSide, const OrderPointer& order);

    std::unordered_map<OrderId, OrderEntry> orders_;  // O(1) lookup by id
    Bids bids_;
    Asks asks_;
};

}  // namespace ob

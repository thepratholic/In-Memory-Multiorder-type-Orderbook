#pragma once

#include <list>
#include <memory>

#include "orderbook/order_type.hpp"
#include "orderbook/side.hpp"
#include "orderbook/types.hpp"

namespace ob {

class Order {
public:
    Order(OrderId orderId, OrderType orderType, Side side, Price price, Quantity quantity)
        : orderId_{orderId},
          orderType_{orderType},
          side_{side},
          price_{price},
          initialQuantity_{quantity},
          remainingQuantity_{quantity} {}

    [[nodiscard]] OrderId   getOrderId() const noexcept { return orderId_; }
    [[nodiscard]] OrderType getOrderType() const noexcept { return orderType_; }
    [[nodiscard]] Side      getSide() const noexcept { return side_; }
    [[nodiscard]] Price     getPrice() const noexcept { return price_; }
    [[nodiscard]] Quantity  getInitialQuantity() const noexcept { return initialQuantity_; }
    [[nodiscard]] Quantity  getRemainingQuantity() const noexcept { return remainingQuantity_; }
    [[nodiscard]] Quantity  getFilledQuantity() const noexcept {
        return initialQuantity_ - remainingQuantity_;
    }
    [[nodiscard]] bool isFilled() const noexcept { return remainingQuantity_ == 0; }

    // Reduces the remaining quantity. Throws std::logic_error on overfill.
    void fill(Quantity quantity);

private:
    OrderId   orderId_;
    OrderType orderType_;
    Side      side_;
    Price     price_;
    Quantity  initialQuantity_;
    Quantity  remainingQuantity_;
};

// Orders are shared between the id lookup table and the price levels.
using OrderPointer  = std::shared_ptr<Order>;
// std::list: iterators stay valid when other elements are added/removed,
// which gives us O(1) cancellation from the middle of a price level.
using OrderPointers = std::list<OrderPointer>;

}  // namespace ob

#pragma once

#include <memory>

#include "orderbook/order.hpp"

namespace ob {

// A request to replace an existing order's side / price / quantity.
// The order type is carried over from the original order.
class OrderModify {
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
        : orderId_{orderId}, side_{side}, price_{price}, quantity_{quantity} {}

    [[nodiscard]] OrderId  getOrderId() const noexcept { return orderId_; }
    [[nodiscard]] Side     getSide() const noexcept { return side_; }
    [[nodiscard]] Price    getPrice() const noexcept { return price_; }
    [[nodiscard]] Quantity getQuantity() const noexcept { return quantity_; }

    [[nodiscard]] OrderPointer toOrderPointer(OrderType type) const {
        return std::make_shared<Order>(orderId_, type, side_, price_, quantity_);
    }

private:
    OrderId  orderId_;
    Side     side_;
    Price    price_;
    Quantity quantity_;
};

}  // namespace ob

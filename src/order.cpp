#include "orderbook/order.hpp"

#include <stdexcept>
#include <string>

namespace ob {

void Order::fill(Quantity quantity) {
    if (quantity > remainingQuantity_) {
        throw std::logic_error("Order " + std::to_string(orderId_) +
                               " cannot be filled for more than its remaining quantity");
    }
    remainingQuantity_ -= quantity;
}

}  // namespace ob

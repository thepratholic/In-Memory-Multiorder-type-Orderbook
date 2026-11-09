// Demo: feeds a small sequence of orders into the book and prints the trades.

#include <iostream>
#include <memory>
#include <vector>

#include "orderbook/helpers.hpp"
#include "orderbook/orderbook.hpp"

int main() {
    using namespace ob;

    OrderBook book;

    const std::vector<OrderPointer> orders = {
        std::make_shared<Order>(0, OrderType::GoodTillCancel, Side::Buy,  93.02f,  5),
        std::make_shared<Order>(1, OrderType::GoodTillCancel, Side::Buy,  100.02f, 5),
        std::make_shared<Order>(2, OrderType::Market,         Side::Sell, kNoPrice, 12),
        std::make_shared<Order>(3, OrderType::GoodTillCancel, Side::Sell, 100.02f, 10),
        std::make_shared<Order>(4, OrderType::FillAndKill,    Side::Buy,  100.4f,  27),
        std::make_shared<Order>(5, OrderType::GoodTillCancel, Side::Sell, 90.0f,   30),
        std::make_shared<Order>(6, OrderType::FillOrKill,     Side::Buy,  100.02f, 20),
        std::make_shared<Order>(7, OrderType::GoodTillCancel, Side::Sell, 99.1f,   10),
    };

    for (const OrderPointer& order : orders) {
        if (const auto trades = book.addOrder(order)) {
            printTrades(*trades);
        }
    }

    std::cout << "OrderBook size: " << book.size() << '\n';
    book.print();
    return 0;
}

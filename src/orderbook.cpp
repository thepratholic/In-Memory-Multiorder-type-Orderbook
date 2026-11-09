#include "orderbook/orderbook.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>

namespace ob {

namespace {

// Removes one order from its price level and drops the level if it became empty.
template <class Book>
void eraseFromLevel(Book& book, Price price, OrderPointers::iterator location) {
    const auto levelIt = book.find(price);
    if (levelIt == book.end()) {
        return;
    }
    levelIt->second.erase(location);
    if (levelIt->second.empty()) {
        book.erase(levelIt);
    }
}

LevelInfo makeLevelInfo(Price price, const OrderPointers& orders) {
    Quantity quantity = 0;
    for (const OrderPointer& order : orders) {
        quantity += order->getRemainingQuantity();
    }
    return LevelInfo{price, quantity};
}

}  // namespace

// ---------------------------------------------------------------------------
// Matching helpers
// ---------------------------------------------------------------------------

bool OrderBook::canMatch(Side side, Price price) const {
    if (side == Side::Buy) {
        return !asks_.empty() && price >= asks_.begin()->first;
    }
    return !bids_.empty() && price <= bids_.begin()->first;
}

bool OrderBook::canFullyMatch(const Order& order) const {
    const bool     isMarket = order.getOrderType() == OrderType::Market;
    const Quantity wanted   = order.getRemainingQuantity();

    // Sums the liquidity on the opposite side that this order is allowed to hit.
    // Levels are visited best price first, so we can stop at the first level
    // that is outside the order's limit price.
    const auto enoughLiquidity = [&](const auto& book, auto withinLimit) {
        std::uint64_t available = 0;
        for (const auto& [price, level] : book) {
            if (!isMarket && !withinLimit(price)) {
                break;
            }
            for (const OrderPointer& resting : level) {
                available += resting->getRemainingQuantity();
            }
            if (available >= wanted) {
                return true;
            }
        }
        return false;
    };

    if (order.getSide() == Side::Buy) {
        return enoughLiquidity(asks_, [&](Price p) { return p <= order.getPrice(); });
    }
    return enoughLiquidity(bids_, [&](Price p) { return p >= order.getPrice(); });
}

// Matches crossed resting orders: best bid vs best ask, oldest order first.
Trades OrderBook::matchOrders() {
    Trades trades;

    while (!bids_.empty() && !asks_.empty()) {
        const auto bidLevelIt = bids_.begin();
        const auto askLevelIt = asks_.begin();

        if (bidLevelIt->first < askLevelIt->first) {
            break;  // book is no longer crossed
        }

        OrderPointers& bids = bidLevelIt->second;
        OrderPointers& asks = askLevelIt->second;

        // Copy the shared_ptrs: the list nodes may be popped below.
        const OrderPointer bid = bids.front();
        const OrderPointer ask = asks.front();

        const Quantity quantity = std::min(bid->getRemainingQuantity(), ask->getRemainingQuantity());

        bid->fill(quantity);
        ask->fill(quantity);

        trades.emplace_back(TradeInfo{bid->getOrderId(), bid->getPrice(), quantity},
                            TradeInfo{ask->getOrderId(), ask->getPrice(), quantity});

        if (bid->isFilled()) {
            orders_.erase(bid->getOrderId());
            bids.pop_front();
        }
        if (ask->isFilled()) {
            orders_.erase(ask->getOrderId());
            asks.pop_front();
        }

        if (bids.empty()) {
            bids_.erase(bidLevelIt);
        }
        if (asks.empty()) {
            asks_.erase(askLevelIt);
        }
    }

    return trades;
}

// Market orders never rest in the book: they sweep the opposite side,
// best price first, until they are filled or liquidity runs out.
template <class Book>
Trades OrderBook::matchMarketOrder(Book& oppositeSide, const OrderPointer& order) {
    Trades trades;

    while (!oppositeSide.empty() && !order->isFilled()) {
        const auto      levelIt = oppositeSide.begin();
        OrderPointers&  level   = levelIt->second;
        const OrderPointer resting = level.front();

        const Quantity quantity =
            std::min(resting->getRemainingQuantity(), order->getRemainingQuantity());

        resting->fill(quantity);
        order->fill(quantity);

        // Market orders trade at the resting order's price.
        const Price     price = resting->getPrice();
        const TradeInfo incoming{order->getOrderId(), price, quantity};
        const TradeInfo existing{resting->getOrderId(), price, quantity};

        if (order->getSide() == Side::Buy) {
            trades.emplace_back(incoming, existing);  // (bid, ask)
        } else {
            trades.emplace_back(existing, incoming);
        }

        if (resting->isFilled()) {
            orders_.erase(resting->getOrderId());
            level.pop_front();
            if (level.empty()) {
                oppositeSide.erase(levelIt);
            }
        }
    }

    return trades;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::optional<Trades> OrderBook::addOrder(OrderPointer order) {
    if (!order || order->isFilled()) {
        return std::nullopt;  // null pointer or zero quantity
    }
    if (orders_.contains(order->getOrderId())) {
        return std::nullopt;  // duplicate id
    }

    const OrderType type = order->getOrderType();

    if (type == OrderType::Market) {
        return order->getSide() == Side::Buy ? matchMarketOrder(asks_, order)
                                             : matchMarketOrder(bids_, order);
    }

    if (std::isnan(order->getPrice())) {
        return std::nullopt;  // only market orders may omit a price
    }
    if (type == OrderType::FillAndKill && !canMatch(order->getSide(), order->getPrice())) {
        return std::nullopt;
    }
    if (type == OrderType::FillOrKill && !canFullyMatch(*order)) {
        return std::nullopt;
    }

    // Append at the back of its price level (time priority) and remember where.
    OrderPointers::iterator location;
    if (order->getSide() == Side::Buy) {
        OrderPointers& level = bids_[order->getPrice()];
        level.push_back(order);
        location = std::prev(level.end());
    } else {
        OrderPointers& level = asks_[order->getPrice()];
        level.push_back(order);
        location = std::prev(level.end());
    }
    orders_.emplace(order->getOrderId(), OrderEntry{order, location});

    Trades trades = matchOrders();

    // FillAndKill: whatever could not be matched immediately is cancelled.
    // (No-op if the order was completely filled and is already gone.)
    if (type == OrderType::FillAndKill) {
        cancelOrder(order->getOrderId());
    }

    return trades;
}

void OrderBook::cancelOrder(OrderId orderId) {
    const auto it = orders_.find(orderId);
    if (it == orders_.end()) {
        return;
    }

    const OrderEntry entry = it->second;  // copy before erasing the map entry
    orders_.erase(it);

    const Price price = entry.order->getPrice();
    if (entry.order->getSide() == Side::Sell) {
        eraseFromLevel(asks_, price, entry.location);
    } else {
        eraseFromLevel(bids_, price, entry.location);
    }
}

std::optional<Trades> OrderBook::modifyOrder(const OrderModify& modify) {
    const auto it = orders_.find(modify.getOrderId());
    if (it == orders_.end()) {
        return std::nullopt;
    }

    const OrderType type = it->second.order->getOrderType();

    cancelOrder(modify.getOrderId());
    return addOrder(modify.toOrderPointer(type));
}

OrderbookLevelInfos OrderBook::getOrderLevelInfos() const {
    LevelInfos bidInfos;
    LevelInfos askInfos;
    bidInfos.reserve(bids_.size());
    askInfos.reserve(asks_.size());

    for (const auto& [price, orders] : bids_) {
        bidInfos.push_back(makeLevelInfo(price, orders));
    }
    for (const auto& [price, orders] : asks_) {
        askInfos.push_back(makeLevelInfo(price, orders));
    }

    return OrderbookLevelInfos{std::move(bidInfos), std::move(askInfos)};
}

void OrderBook::print() const {
    print(std::cout);
}

void OrderBook::print(std::ostream& os) const {
    const OrderbookLevelInfos infos = getOrderLevelInfos();

    os << "Bids:\n";
    int level = 0;
    for (const LevelInfo& info : infos.getBids()) {
        os << "Level: " << level++ << " -> Price: " << info.price
           << ", Quantity: " << info.quantity << '\n';
    }

    os << "\nAsks:\n";
    level = 0;
    for (const LevelInfo& info : infos.getAsks()) {
        os << "Level: " << level++ << " -> Price: " << info.price
           << ", Quantity: " << info.quantity << '\n';
    }
}

}  // namespace ob

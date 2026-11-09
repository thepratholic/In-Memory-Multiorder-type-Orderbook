#pragma once

#include <iostream>

#include "orderbook/trade.hpp"

namespace ob {

inline void printTrades(const Trades& trades, std::ostream& os = std::cout) {
    for (const auto& trade : trades) {
        const TradeInfo& bid = trade.getBidTrade();
        const TradeInfo& ask = trade.getAskTrade();
        os << "Trade: " << ask.orderId << " -> " << bid.orderId << '\n'
           << "  Bid: price = " << bid.price << ", quantity = " << bid.quantity << '\n'
           << "  Ask: price = " << ask.price << ", quantity = " << ask.quantity << '\n';
    }
}

}  // namespace ob

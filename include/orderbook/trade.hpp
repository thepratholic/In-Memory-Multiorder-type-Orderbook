#pragma once

#include <vector>

#include "orderbook/types.hpp"

namespace ob {

// One side of a trade: which order took part, at what price, for how much.
struct TradeInfo {
    OrderId  orderId;
    Price    price;
    Quantity quantity;
};

// A trade always pairs one bid (buyer) with one ask (seller).
class Trade {
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
        : bidTrade_{bidTrade}, askTrade_{askTrade} {}

    [[nodiscard]] const TradeInfo& getBidTrade() const noexcept { return bidTrade_; }
    [[nodiscard]] const TradeInfo& getAskTrade() const noexcept { return askTrade_; }

private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;

}  // namespace ob

#pragma once

#include <utility>
#include <vector>

#include "orderbook/types.hpp"

namespace ob {

// Aggregated view of one price level (Level-2 / market-by-price data).
struct LevelInfo {
    Price    price;
    Quantity quantity;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderbookLevelInfos {
public:
    OrderbookLevelInfos(LevelInfos bids, LevelInfos asks)
        : bids_{std::move(bids)}, asks_{std::move(asks)} {}

    // Bids are sorted high -> low, asks low -> high (best price first).
    [[nodiscard]] const LevelInfos& getBids() const noexcept { return bids_; }
    [[nodiscard]] const LevelInfos& getAsks() const noexcept { return asks_; }

private:
    LevelInfos bids_;
    LevelInfos asks_;
};

}  // namespace ob

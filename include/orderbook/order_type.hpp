#pragma once

namespace ob {

enum class OrderType {
    GoodTillCancel,  // rests in the book until filled or cancelled
    FillAndKill,     // fills what it can immediately, the remainder is cancelled
    FillOrKill,      // fills completely and immediately, or is rejected entirely
    Market,          // takes the best available prices, never rests in the book
    // GoodForDay,   // (planned) cancelled automatically at the end of the trading day
};

}  // namespace ob

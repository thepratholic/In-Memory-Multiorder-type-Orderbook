# orderbook-cpp

An in-memory **limit order book and matching engine** written in modern C++20.
It matches orders with **price-time (FIFO) priority** and supports multiple order types.

- No external dependencies (standard library only)
- Header + static library layout, built with CMake
- Unit tests that run under AddressSanitizer / UBSan
- O(1) best-price lookup, O(1) cancellation from the middle of a queue

---

## Table of contents

- [Background](#background)
- [Features](#features)
- [Design](#design)
- [Build, run and test](#build-run-and-test)
- [Usage](#usage)
- [Example output](#example-output)
- [Project layout](#project-layout)
- [Limitations and roadmap](#limitations-and-roadmap)
- [Acknowledgements](#acknowledgements)

---

## Background

An **order book** is the list of all outstanding **buy (bid)** and **sell (ask)** orders for one
instrument, organised by price and arrival time. It shows who wants to buy or sell, how much, and
at what price. A **matching engine** pairs crossing orders (a bid priced at or above an ask) into trades.

### Order book levels

| Level | Also called | What it shows |
|-------|-------------|---------------|
| L1 | Top of book | Best bid and best ask only |
| L2 | Market by price | Aggregated quantity at each price level |
| L3 | Market by order | Every individual order |

This project keeps L3 data internally and exposes an **L2 snapshot** via `getOrderLevelInfos()`.

### Matching algorithms

| Algorithm | Rule | Status |
|-----------|------|--------|
| **Price-time priority (FIFO)** | Best price first; at equal price the oldest order first | Implemented |
| Pro-rata | Orders at the same price share the fill proportionally to size | Not implemented |
| Size-time priority | Larger orders first at equal price, then by time | Not implemented |

---

## Features

| Order type | Behaviour |
|------------|-----------|
| `GoodTillCancel` | Rests in the book until filled or cancelled |
| `FillAndKill` | Fills as much as possible immediately; the remainder is cancelled |
| `FillOrKill` | Fills completely and immediately, otherwise the whole order is rejected |
| `Market` | Sweeps the opposite side at the best available prices; never rests |

Operations: `addOrder`, `cancelOrder`, `modifyOrder`, `size`, `getOrderLevelInfos`, `print`.

---

## Design

```
                 unordered_map<OrderId, OrderEntry>      O(1) lookup by id
                                  |
        +-------------------------+--------------------------+
        |                                                    |
 bids_: map<Price, list<Order>, greater>        asks_: map<Price, list<Order>, less>
   100.0 -> [#4][#9]      <- best bid             101.0 -> [#7]        <- best ask
    99.5 -> [#2]                                  101.5 -> [#3][#8]
```

| Choice | Why |
|--------|-----|
| `std::map<Price, OrderPointers, std::greater<>>` for bids, `std::less<>` for asks | Keeps price levels sorted; `begin()` is always the best price |
| `std::list<std::shared_ptr<Order>>` per price level | FIFO queue whose iterators stay valid, so a cancel is an O(1) `erase(iterator)` |
| `std::unordered_map<OrderId, {order, list iterator}>` | O(1) lookup of any order, including its exact position in its queue |
| `std::optional<Trades>` as the return type of `addOrder` | Distinguishes "order rejected" (`nullopt`) from "accepted, no trades" (empty vector) |

### Complexity (P = number of price levels)

| Operation | Cost |
|-----------|------|
| Best bid / best ask | O(1) |
| Add a resting order | O(log P) |
| Cancel an order | O(log P) to find the level, O(1) to unlink |
| Match one pair of orders | O(1) amortised, plus O(log P) when a level is removed |
| `FillOrKill` feasibility check | O(orders on the book within the limit price) |

---

## Build, run and test

Requirements: a C++20 compiler (GCC 11+, Clang 14+, MSVC 2019+) and CMake 3.16+.

```bash
# configure + build (Release by default)
cmake -S . -B build
cmake --build build -j

# run the demo
./build/orderbook_demo

# run the unit tests
ctest --test-dir build --output-on-failure
```

Build with AddressSanitizer + UBSan:

```bash
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DORDERBOOK_SANITIZE=ON
cmake --build build-san -j
ctest --test-dir build-san --output-on-failure
```

CMake options: `ORDERBOOK_BUILD_TESTS` (default `ON`), `ORDERBOOK_SANITIZE` (default `OFF`).

---

## Usage

```cpp
#include <memory>
#include "orderbook/helpers.hpp"
#include "orderbook/orderbook.hpp"

int main() {
    using namespace ob;
    OrderBook book;

    // Order(id, type, side, price, quantity)
    book.addOrder(std::make_shared<Order>(1, OrderType::GoodTillCancel, Side::Buy, 100.0f, 10));

    // A crossing sell order produces trades
    if (auto trades = book.addOrder(
            std::make_shared<Order>(2, OrderType::GoodTillCancel, Side::Sell, 100.0f, 4))) {
        printTrades(*trades);          // "accepted": possibly empty vector of trades
    }                                  // std::nullopt means "rejected"

    book.modifyOrder(OrderModify{1, Side::Buy, 99.5f, 6});   // cancel + re-add
    book.cancelOrder(1);
    book.print();
}
```

---

## Example output

`./build/orderbook_demo` feeds eight orders (GTC, market, fill-and-kill, fill-or-kill) into the book:

```
Trade: 2 -> 1
  Bid: price = 100.02, quantity = 5
  Ask: price = 100.02, quantity = 5
Trade: 2 -> 0
  Bid: price = 93.02, quantity = 5
  Ask: price = 93.02, quantity = 5
Trade: 3 -> 4
  Bid: price = 100.4, quantity = 10
  Ask: price = 100.02, quantity = 10
Trade: 5 -> 6
  Bid: price = 100.02, quantity = 20
  Ask: price = 90, quantity = 20
OrderBook size: 2
Bids:

Asks:
Level: 0 -> Price: 90, Quantity: 10
Level: 1 -> Price: 99.1, Quantity: 10
```

Each trade line reads `Trade: <ask order id> -> <bid order id>`.

---

## Project layout

```
orderbook-cpp/
├── CMakeLists.txt
├── include/orderbook/
│   ├── types.hpp          Price, Quantity, OrderId
│   ├── side.hpp           Side { Buy, Sell }
│   ├── order_type.hpp     OrderType { GoodTillCancel, FillAndKill, FillOrKill, Market }
│   ├── order.hpp          Order, OrderPointer(s)
│   ├── order_modify.hpp   OrderModify
│   ├── trade.hpp          TradeInfo, Trade, Trades
│   ├── level_info.hpp     LevelInfo, OrderbookLevelInfos
│   ├── orderbook.hpp      OrderBook (public API)
│   └── helpers.hpp        printTrades
├── src/
│   ├── order.cpp
│   ├── orderbook.cpp      matching engine
│   └── main.cpp           demo
├── tests/orderbook_tests.cpp
├── docs/RUST_TO_CPP.md    concept map for readers coming from Rust
└── .github/workflows/ci.yml
```

---

## Limitations and roadmap

- Prices are `float`. A production engine would use integer ticks (fixed point) to avoid
  floating-point comparison issues.
- Single-threaded. Concurrent use needs external locking, or one book per thread.
- `GoodForDay` orders are not implemented yet (needs a clock / session model).
- Matching is FIFO only; pro-rata and size-time priority are possible extensions.
- Ideas: benchmark harness (orders/second), replaying a real market data file, stop orders,
  an intrusive list / memory pool to avoid per-order allocations.

---

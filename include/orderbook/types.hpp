#pragma once

#include <cstdint>
#include <limits>

namespace ob {

// NOTE: `float` keeps parity with the original design. Production engines
// normally store prices as integer ticks to avoid floating-point comparisons.
using Price    = float;
using Quantity = std::uint32_t;
using OrderId  = std::uint64_t;

// Market orders carry no limit price.
inline constexpr Price kNoPrice = std::numeric_limits<Price>::quiet_NaN();

}  // namespace ob

#pragma once

#include "Types.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace bench {

inline const Ticker kTicker = "BENCH";

// Google Benchmark only computes mean/median/stddev out of the box; this plugs
// a p90 into ->ComputeStatistics() from the per-repetition timings so the
// results report a real tail number instead of just the average.
inline double Percentile90(const std::vector<double>& values) {
    std::vector<double> sorted(values);
    std::sort(sorted.begin(), sorted.end());
    std::size_t idx = static_cast<std::size_t>(0.9 * static_cast<double>(sorted.size() - 1));
    return sorted[idx];
}

} // namespace bench

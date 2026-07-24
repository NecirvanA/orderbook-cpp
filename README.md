# orderbook-cpp

A limit order book and matching engine in C++20: price-time priority matching, GTC/IOC/FOK/Market/Stop/Stop-Limit
order types, and multi-ticker support. Benchmarked and profiled with Google Benchmark; see [Performance](#performance)
for methodology and results.

## Features

- **Order types**: `MARKET`, `LIMIT`, `STOP`, `STOP_LIMIT`, with `DAY` / `GOOD_TILL_CANCEL` / `IMMEDIATE_OR_CANCEL` /
  `FILL_OR_KILL` / `GOOD_TILL_DATE` time-in-force
- **Matching**: price-time priority (FIFO within a price level), partial fills, multi-level sweeps
- **Order lookup**: O(1) by order ID via a hash map; O(log n) price-level access via a sorted map (n = number of
  distinct price levels, not resting orders)
- **Cancellation**: O(1) removal from a price level via a stored list iterator, independent of level depth
- Independent order book per ticker inside a single `MatchingEngine`

## Build

```bash
cmake -S . -B build && cmake --build build
```

Tests (GoogleTest, fetched via CMake `FetchContent`):

```bash
cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build --target orderbook_tests
./build/tests/orderbook_tests
```

Benchmarks (Google Benchmark, also fetched via `FetchContent`) — **must** be built in `Release` or the numbers below
are meaningless:

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build-bench --target orderbook_benchmarks
./build-bench/benchmarks/orderbook_benchmarks
```

## Performance

### Methodology

- **Hardware**: Intel Core i5-13500 (20 logical CPUs), 24 MiB L3
- **Toolchain**: GCC 16.1.1, `-O3 -DNDEBUG -std=gnu++20` (CMake `Release`), Google Benchmark v1.9.1
- **Statistics**: every data point is 10 independent repetitions of 30 iterations each; median is reported below,
  with mean/stddev/p90 also collected in raw output.
- **Isolation**: per-iteration setup (building the book/engine to the target size) and object teardown both run
  while the timer is paused (`state.PauseTiming()`/`ResumeTiming()`), so only the operation under test is measured.
- **Caveat**: CPU frequency scaling was left enabled (no governor pinning, no core isolation), so treat the raw
  microsecond figures as indicative, not lab-grade. The relative comparisons across depths are the reliable part.

Raw `--benchmark_filter` output for every number below is reproducible with the build command above.

### Cancellation

Cancel one order sitting in the middle of a price level holding `N` resting orders:

| Orders at that price level | Median time |
|---:|---:|
| 10      | 0.34 µs |
| 100     | 0.38 µs |
| 1,000   | 0.44 µs |
| 10,000  | 0.91 µs |
| 100,000 | 2.53 µs |

Cost stays flat within measurement noise as depth grows, consistent with O(1) removal.

### Insertion

Inserting `N` resting limit orders, each at a distinct price level, into an empty book (stresses the `std::map`
price-level insert path):

| Book depth | Median time | Throughput |
|---:|---:|---:|
| 10      | 1.28 µs | 7.8M orders/sec |
| 100     | 11.0 µs | 9.1M orders/sec |
| 1,000   | 66.4 µs | 15.1M orders/sec |
| 10,000  | 714 µs  | 14.1M orders/sec |
| 100,000 | 11.9 ms | 8.4M orders/sec |

### Matching throughput vs. book depth

One marketable order sweeps and fully fills `N` resting orders sitting at `N` distinct price levels:

| Resting sell levels swept | Median time | Throughput |
|---:|---:|---:|
| 10      | 1.41 µs | 7.1M matches/sec |
| 100     | 12.1 µs | 8.3M matches/sec |
| 1,000   | 118 µs  | 8.5M matches/sec |
| 10,000  | 1.23 ms | 8.1M matches/sec |
| 100,000 | 13.9 ms | 7.2M matches/sec |

Throughput stays flat (7-9M matches/sec) across four orders of magnitude of book depth, since sweeping levels is
bounded by `std::map` iteration rather than a linear rescan.

### Fill-or-kill rejection cost

A `FILL_OR_KILL` order sized just over the total resting liquidity, guaranteed to be rejected. This isolates
`canFillCompletely`'s liquidity-scan cost from actual trade execution:

| Resting sell levels | Median time | Throughput |
|---:|---:|---:|
| 10     | 0.39 µs | 2.6M rejections/sec |
| 100    | 0.79 µs | 1.3M rejections/sec |
| 1,000  | 4.9 µs  | 208K rejections/sec |
| 10,000 | 77.1 µs | 13.0K rejections/sec |

This scan is O(depth); `canFillCompletely` walks every resting order on the opposing side to determine there
isn't enough liquidity. A running "total resting quantity per side" counter, updated incrementally on add/fill/cancel,
would make this O(1) at the cost of extra bookkeeping on every mutation.

### Mixed realistic workload

One interleaved stream of 70% new resting limit orders, 20% cancels, 10% marketable (IOC) orders that cross the
touch, against a continuously-churning book:

**5.29M ops/sec** (1.87 ms median per 10,000 ops).
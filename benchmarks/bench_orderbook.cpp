#include "bench_common.h"

#include "Orderbook.h"
#include "Order.h"
#include "OrderType.h"
#include "Types.h"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

static void BM_AddOrder_RestingLimit(benchmark::State& state) {
    const std::int64_t depth = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        {
            Orderbook book(bench::kTicker);
            std::vector<Order> storage;
            storage.reserve(static_cast<std::size_t>(depth));
            for (std::int64_t i = 0; i < depth; ++i) {
                storage.emplace_back(OrderType::LIMIT, static_cast<OrderId>(i + 1), Side::BUY,
                                      static_cast<Price>(1000 + i), Quantity{10}, TimeInForce::GOOD_TILL_CANCEL);
            }
            state.ResumeTiming();

            for (auto& order : storage) {
                book.addOrder(&order);
            }

            state.PauseTiming();
        }
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * depth);
}
BENCHMARK(BM_AddOrder_RestingLimit)
    ->RangeMultiplier(10)->Range(10, 100000)
    ->Iterations(30)
    ->Repetitions(10)->ReportAggregatesOnly(true)
    ->ComputeStatistics("p90", bench::Percentile90)
    ->Unit(benchmark::kMicrosecond);


static void BM_CancelOrder_SameLevelDepth(benchmark::State& state) {
    const std::int64_t depth = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        {
            Orderbook book(bench::kTicker);
            std::vector<Order> storage;
            storage.reserve(static_cast<std::size_t>(depth));
            for (std::int64_t i = 0; i < depth; ++i) {
                storage.emplace_back(OrderType::LIMIT, static_cast<OrderId>(i + 1), Side::BUY,
                                      Price{1000}, Quantity{10}, TimeInForce::GOOD_TILL_CANCEL);
                book.addOrder(&storage.back());
            }
            OrderId target = storage[static_cast<std::size_t>(depth / 2)].getOrderId();
            state.ResumeTiming();

            book.cancelOrder(target);

            state.PauseTiming();
        }
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CancelOrder_SameLevelDepth)
    ->RangeMultiplier(10)->Range(10, 100000)
    ->Iterations(30)
    ->Repetitions(10)->ReportAggregatesOnly(true)
    ->ComputeStatistics("p90", bench::Percentile90)
    ->Unit(benchmark::kMicrosecond);

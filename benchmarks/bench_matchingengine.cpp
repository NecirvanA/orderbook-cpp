#include "bench_common.h"

#include "MatchingEngine.h"
#include "OrderType.h"
#include "Types.h"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <deque>
#include <random>


static void BM_Matching_CrossBook(benchmark::State& state) {
    const std::int64_t bookDepth = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        {
            MatchingEngine engine;
            OrderId nextId = 1;
            for (std::int64_t i = 0; i < bookDepth; ++i) {
                engine.addOrder(bench::kTicker, OrderType::LIMIT, nextId++, Side::SELL,
                                 static_cast<Price>(1000 + i), Quantity{1}, TimeInForce::GOOD_TILL_CANCEL);
            }
            state.ResumeTiming();

            auto trades = engine.addOrder(bench::kTicker, OrderType::LIMIT, nextId++, Side::BUY,
                                           static_cast<Price>(1000 + bookDepth - 1),
                                           static_cast<Quantity>(bookDepth), TimeInForce::GOOD_TILL_CANCEL);
            benchmark::DoNotOptimize(trades);

            state.PauseTiming();
        }
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * bookDepth);
}

BENCHMARK(BM_Matching_CrossBook)
    ->RangeMultiplier(10)->Range(10, 100000)
    ->Iterations(30)
    ->Repetitions(10)->ReportAggregatesOnly(true)
    ->ComputeStatistics("p90", bench::Percentile90)
    ->Unit(benchmark::kMicrosecond);

static void BM_FillOrKill_Rejection(benchmark::State& state) {
    const std::int64_t bookDepth = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        {
            MatchingEngine engine;
            OrderId nextId = 1;
            for (std::int64_t i = 0; i < bookDepth; ++i) {
                engine.addOrder(bench::kTicker, OrderType::LIMIT, nextId++, Side::SELL,
                                 static_cast<Price>(1000 + i), Quantity{1}, TimeInForce::GOOD_TILL_CANCEL);
            }
            state.ResumeTiming();

            auto trades = engine.addOrder(bench::kTicker, OrderType::LIMIT, nextId++, Side::BUY,
                                           static_cast<Price>(1000 + bookDepth - 1),
                                           static_cast<Quantity>(bookDepth + 1), TimeInForce::FILL_OR_KILL);
            benchmark::DoNotOptimize(trades);

            state.PauseTiming();
        }
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_FillOrKill_Rejection)
    ->RangeMultiplier(10)->Range(10, 10000)
    ->Iterations(30)
    ->Repetitions(10)->ReportAggregatesOnly(true)
    ->ComputeStatistics("p90", bench::Percentile90)
    ->Unit(benchmark::kMicrosecond);

static void BM_MixedWorkload(benchmark::State& state) {
    constexpr int kOpsPerIteration = 10000;

    for (auto _ : state) {
        state.PauseTiming();
        {
            MatchingEngine engine;
            std::mt19937 rng(42);
            std::uniform_int_distribution<int> opPicker(0, 99);
            std::uniform_int_distribution<int> jitter(-20, 20);
            OrderId nextId = 1;
            std::deque<OrderId> restingBuyIds;
            std::deque<OrderId> restingSellIds;

            for (int i = 0; i < 500; ++i) {
                OrderId buyId = nextId++;
                engine.addOrder(bench::kTicker, OrderType::LIMIT, buyId, Side::BUY,
                                 static_cast<Price>(925 + jitter(rng)), Quantity{10}, TimeInForce::GOOD_TILL_CANCEL);
                restingBuyIds.push_back(buyId);

                OrderId sellId = nextId++;
                engine.addOrder(bench::kTicker, OrderType::LIMIT, sellId, Side::SELL,
                                 static_cast<Price>(1075 + jitter(rng)), Quantity{10}, TimeInForce::GOOD_TILL_CANCEL);
                restingSellIds.push_back(sellId);
            }
            state.ResumeTiming();

            for (int i = 0; i < kOpsPerIteration; ++i) {
                int roll = opPicker(rng);
                OrderId id = nextId++;

                if (roll < 70) {
                    Side side = (roll % 2 == 0) ? Side::BUY : Side::SELL;
                    Price price = (side == Side::BUY)
                        ? static_cast<Price>(925 + jitter(rng))
                        : static_cast<Price>(1075 + jitter(rng));
                    engine.addOrder(bench::kTicker, OrderType::LIMIT, id, side, price, Quantity{10}, TimeInForce::GOOD_TILL_CANCEL);
                    (side == Side::BUY ? restingBuyIds : restingSellIds).push_back(id);
                } else if (roll < 90) {
                    auto& pool = (roll % 2 == 0) ? restingBuyIds : restingSellIds;
                    if (!pool.empty()) {
                        OrderId victim = pool.front();
                        pool.pop_front();
                        try {
                            engine.cancelOrder(victim);
                        } catch (const std::invalid_argument&) {
                        }
                    }
                } else {
                    Side side = (roll % 2 == 0) ? Side::BUY : Side::SELL;
                    Price price = (side == Side::BUY) ? Price{1100} : Price{900};
                    auto trades = engine.addOrder(bench::kTicker, OrderType::LIMIT, id, side, price, Quantity{5}, TimeInForce::IMMEDIATE_OR_CANCEL);
                    benchmark::DoNotOptimize(trades);
                }
            }

            state.PauseTiming();
        }
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * kOpsPerIteration);
}
BENCHMARK(BM_MixedWorkload)
    ->Iterations(20)
    ->Repetitions(10)->ReportAggregatesOnly(true)
    ->ComputeStatistics("p90", bench::Percentile90)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
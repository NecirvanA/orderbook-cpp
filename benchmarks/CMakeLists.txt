add_executable(orderbook_benchmarks
    bench_orderbook.cpp
    bench_matchingengine.cpp
)
target_link_libraries(orderbook_benchmarks PRIVATE orderbook_headers benchmark::benchmark)
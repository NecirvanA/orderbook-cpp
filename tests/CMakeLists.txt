add_executable(orderbook_tests
    test_orderbook.cpp
    test_order.cpp
    test_matchingengine.cpp
)
target_link_libraries(orderbook_tests PRIVATE orderbook_headers gtest_main)

include(GoogleTest)
gtest_discover_tests(orderbook_tests)
#include "Order.h"
#include <gtest/gtest.h>
#include <ctime>

class OrderTest : public ::testing::Test {
protected:
    // A standard resting limit buy order to reuse across tests
    Order makeLimitOrder(Quantity qty = 100) {
        return Order(OrderType::LIMIT, /*orderId=*/1, Side::BUY, /*price=*/1000, qty, TimeInForce::DAY);
    }
};

// --- Construction: limit order ---

TEST_F(OrderTest, LimitOrderConstructorSetsAllFields) {
    Order o(OrderType::LIMIT, 42, Side::SELL, 1050, 200, TimeInForce::DAY);

    EXPECT_EQ(o.getOrderType(), OrderType::LIMIT);
    EXPECT_EQ(o.getOrderId(), 42);
    EXPECT_EQ(o.getSide(), Side::SELL);
    EXPECT_EQ(o.getPrice(), 1050);
    EXPECT_EQ(o.getRemainingQty(), 200);
    EXPECT_EQ(o.getInitialQty(), 200);
    EXPECT_EQ(o.getTimeInForce(), TimeInForce::DAY);

}

TEST_F(OrderTest, LimitOrderStartsUnfilled) {
    Order o = makeLimitOrder(100);
    EXPECT_FALSE(o.isFilled());
}

TEST_F(OrderTest, LimitOrderSetsTimestampNearNow) {
    time_t before = time(nullptr);
    Order o = makeLimitOrder();
    time_t after = time(nullptr);

    EXPECT_GE(o.getTimeStamp(), before);
    EXPECT_LE(o.getTimeStamp(), after);
}

// --- Construction: market order (delegating constructor) ---

TEST_F(OrderTest, MarketOrderConstructorSetsTypeAndInvalidPrice) {
    Order o(7, Side::BUY, 50, TimeInForce::DAY);

    EXPECT_EQ(o.getOrderType(), OrderType::MARKET);
    EXPECT_EQ(o.getOrderId(), 7);
    EXPECT_EQ(o.getSide(), Side::BUY);
    EXPECT_EQ(o.getPrice(), Constants::InvalidPrice);
    EXPECT_EQ(o.getRemainingQty(), 50);
}

// --- Validation ---

TEST_F(OrderTest, ZeroQuantityThrows) {
    EXPECT_THROW(Order(OrderType::LIMIT, 1, Side::BUY, 1000, 0, TimeInForce::DAY), std::invalid_argument);
}

TEST_F(OrderTest, MarketOrderZeroQuantityThrows) {
    EXPECT_THROW(Order(1, Side::BUY, 0, TimeInForce::DAY), std::invalid_argument);
}

TEST_F(OrderTest, LimitOrderWithMarketSentinelPriceThrows) {
    EXPECT_THROW(Order(OrderType::LIMIT, 1, Side::BUY, Constants::InvalidPrice, 10, TimeInForce::DAY), std::invalid_argument);
}

// --- Construction: stop / stop-limit orders ---

TEST_F(OrderTest, StopOrderWithoutStopPriceThrows) {
    // no stopPrice given -> defaults to the sentinel, which isn't a valid trigger
    EXPECT_THROW(Order(OrderType::STOP, 1, Side::BUY, Constants::InvalidPrice, 10, TimeInForce::DAY), std::invalid_argument);
}

TEST_F(OrderTest, StopOrderWithLimitPriceThrows) {
    // a STOP becomes a MARKET order once triggered, so it can't carry a limit price too
    EXPECT_THROW(Order(OrderType::STOP, 1, Side::BUY, 1000, 10, TimeInForce::DAY, 950), std::invalid_argument);
}

TEST_F(OrderTest, StopOrderWithStopPriceConstructsSuccessfully) {
    Order o(OrderType::STOP, 1, Side::BUY, Constants::InvalidPrice, 10, TimeInForce::DAY, 950);

    EXPECT_EQ(o.getOrderType(), OrderType::STOP);
    EXPECT_EQ(o.getPrice(), Constants::InvalidPrice);
    EXPECT_EQ(o.getStopPrice(), 950);
}

TEST_F(OrderTest, StopLimitOrderMissingStopPriceThrows) {
    EXPECT_THROW(Order(OrderType::STOP_LIMIT, 1, Side::BUY, 1000, 10, TimeInForce::DAY), std::invalid_argument);
}

TEST_F(OrderTest, StopLimitOrderMissingLimitPriceThrows) {
    EXPECT_THROW(Order(OrderType::STOP_LIMIT, 1, Side::BUY, Constants::InvalidPrice, 10, TimeInForce::DAY, 950), std::invalid_argument);
}

TEST_F(OrderTest, StopLimitOrderWithBothPricesConstructsSuccessfully) {
    Order o(OrderType::STOP_LIMIT, 1, Side::BUY, 1000, 10, TimeInForce::DAY, 950);

    EXPECT_EQ(o.getOrderType(), OrderType::STOP_LIMIT);
    EXPECT_EQ(o.getPrice(), 1000);
    EXPECT_EQ(o.getStopPrice(), 950);
}

TEST_F(OrderTest, LimitOrderWithStopPriceThrows) {
    // stopPrice only makes sense for STOP/STOP_LIMIT
    EXPECT_THROW(Order(OrderType::LIMIT, 1, Side::BUY, 1000, 10, TimeInForce::DAY, 950), std::invalid_argument);
}

TEST_F(OrderTest, MarketOrderWithStopPriceThrows) {
    EXPECT_THROW(Order(OrderType::MARKET, 1, Side::BUY, Constants::InvalidPrice, 10, TimeInForce::DAY, 950), std::invalid_argument);
}

// --- fill() behaviour ---

TEST_F(OrderTest, PartialFillReducesRemainingQty) {
    Order o = makeLimitOrder(100);
    o.fill(30);
    EXPECT_EQ(o.getRemainingQty(), 70);
    EXPECT_FALSE(o.isFilled());
}

TEST_F(OrderTest, FullFillMarksOrderFilled) {
    Order o = makeLimitOrder(100);
    o.fill(100);
    EXPECT_EQ(o.getRemainingQty(), 0);
    EXPECT_TRUE(o.isFilled());
}

TEST_F(OrderTest, MultiplePartialFillsAccumulate) {
    Order o = makeLimitOrder(100);
    o.fill(40);
    o.fill(40);
    EXPECT_EQ(o.getRemainingQty(), 20);
    o.fill(20);
    EXPECT_TRUE(o.isFilled());
}

TEST_F(OrderTest, OverfillThrowsLogicError) {
    Order o = makeLimitOrder(100);
    EXPECT_THROW(o.fill(101), std::logic_error);
}

TEST_F(OrderTest, OverfillAfterPartialFillThrows) {
    Order o = makeLimitOrder(100);
    o.fill(60);
    EXPECT_THROW(o.fill(41), std::logic_error);
}

TEST_F(OrderTest, FillingWithZeroLeavesQuantityUnchanged) {
    Order o = makeLimitOrder(100);
    o.fill(0);
    EXPECT_EQ(o.getRemainingQty(), 100);
    EXPECT_FALSE(o.isFilled());
}

TEST_F(OrderTest, FailedOverfillDoesNotMutateState) {
    Order o = makeLimitOrder(100);
    EXPECT_THROW(o.fill(150), std::logic_error);
    // remainingQty should be untouched since fill() checks before mutating
    EXPECT_EQ(o.getRemainingQty(), 100);
}
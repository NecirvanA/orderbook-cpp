#include "MatchingEngine.h"
#include <gtest/gtest.h>

class MatchingEngineTest : public ::testing::Test {
protected:
    MatchingEngine engine;

    std::vector<Trade> buy(OrderId id, Price price, Quantity qty, TimeInForce tif = TimeInForce::GOOD_TILL_CANCEL, Ticker ticker = "AAPL") {
        return engine.addOrder(ticker, OrderType::LIMIT, id, Side::BUY, price, qty, tif);
    }
    std::vector<Trade> sell(OrderId id, Price price, Quantity qty, TimeInForce tif = TimeInForce::GOOD_TILL_CANCEL, Ticker ticker = "AAPL") {
        return engine.addOrder(ticker, OrderType::LIMIT, id, Side::SELL, price, qty, tif);
    }
    std::vector<Trade> marketBuy(OrderId id, Quantity qty, TimeInForce tif = TimeInForce::GOOD_TILL_CANCEL, Ticker ticker = "AAPL") {
        return engine.addOrder(ticker, OrderType::MARKET, id, Side::BUY, Constants::InvalidPrice, qty, tif);
    }
    std::vector<Trade> marketSell(OrderId id, Quantity qty, TimeInForce tif = TimeInForce::GOOD_TILL_CANCEL, Ticker ticker = "AAPL") {
        return engine.addOrder(ticker, OrderType::MARKET, id, Side::SELL, Constants::InvalidPrice, qty, tif);
    }
    std::vector<Trade> stopBuy(OrderId id, Price stopPrice, Quantity qty, TimeInForce tif = TimeInForce::GOOD_TILL_CANCEL, Ticker ticker = "AAPL") {
        return engine.addOrder(ticker, OrderType::STOP, id, Side::BUY, Constants::InvalidPrice, qty, tif, stopPrice);
    }
    std::vector<Trade> stopSell(OrderId id, Price stopPrice, Quantity qty, TimeInForce tif = TimeInForce::GOOD_TILL_CANCEL, Ticker ticker = "AAPL") {
        return engine.addOrder(ticker, OrderType::STOP, id, Side::SELL, Constants::InvalidPrice, qty, tif, stopPrice);
    }
    std::vector<Trade> stopLimitBuy(OrderId id, Price limitPrice, Price stopPrice, Quantity qty, TimeInForce tif = TimeInForce::GOOD_TILL_CANCEL, Ticker ticker = "AAPL") {
        return engine.addOrder(ticker, OrderType::STOP_LIMIT, id, Side::BUY, limitPrice, qty, tif, stopPrice);
    }
};

// --- Orderbook bookkeeping ---

TEST_F(MatchingEngineTest, HasOrderbookFalseBeforeFirstOrder) {
    EXPECT_FALSE(engine.hasOrderbook("AAPL"));
}

TEST_F(MatchingEngineTest, HasOrderbookTrueAfterFirstOrder) {
    buy(1, 100, 10);
    EXPECT_TRUE(engine.hasOrderbook("AAPL"));
}

TEST_F(MatchingEngineTest, DifferentTickersGetIndependentOrderbooks) {
    buy(1, 100, 10, TimeInForce::GOOD_TILL_CANCEL, "AAPL");
    EXPECT_FALSE(engine.hasOrderbook("MSFT"));

    auto trades = sell(2, 100, 10, TimeInForce::GOOD_TILL_CANCEL, "MSFT");
    EXPECT_TRUE(trades.empty());
    EXPECT_TRUE(engine.hasOrderbook("MSFT"));
}

TEST_F(MatchingEngineTest, DuplicateOrderIdThrows) {
    buy(1, 100, 10);
    EXPECT_THROW(sell(1, 100, 5), std::invalid_argument);
}

// --- No-match resting orders ---

TEST_F(MatchingEngineTest, NonCrossingOrdersProduceNoTrades) {
    auto t1 = buy(1, 100, 10);
    auto t2 = sell(2, 105, 10);

    EXPECT_TRUE(t1.empty());
    EXPECT_TRUE(t2.empty());
    EXPECT_EQ(engine.getOrder(1).getRemainingQty(), 10);
    EXPECT_EQ(engine.getOrder(2).getRemainingQty(), 10);
}

// --- Crossing / matching ---

TEST_F(MatchingEngineTest, CrossingOrdersProduceOneTrade) {
    buy(1, 100, 10);
    auto trades = sell(2, 95, 10);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].getQty(), 10u);
    EXPECT_EQ(trades[0].getBuyer(), 1u);
    EXPECT_EQ(trades[0].getSeller(), 2u);
}

TEST_F(MatchingEngineTest, TradeExecutesAtRestingMakerPrice) {
    buy(1, 100, 10);
    auto trades = sell(2, 95, 10);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].getPrice(), 100);
}

TEST_F(MatchingEngineTest, PartialFillLeavesRemainderResting) {
    buy(1, 100, 10);
    auto trades = sell(2, 100, 4);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].getQty(), 4u);
    EXPECT_EQ(engine.getOrder(1).getRemainingQty(), 6u);
    EXPECT_THROW(engine.getOrder(2), std::out_of_range);
}

TEST_F(MatchingEngineTest, FullyFilledRestingOrderIsRemoved) {
    buy(1, 100, 10);
    sell(2, 100, 10);

    EXPECT_THROW(engine.getOrder(1), std::out_of_range);
    EXPECT_THROW(engine.getOrder(2), std::out_of_range);
}

TEST_F(MatchingEngineTest, IncomingOrderSweepsMultiplePriceLevels) {
    sell(1, 100, 5);
    sell(2, 101, 5);
    sell(3, 102, 5);

    auto trades = buy(4, 102, 15);

    ASSERT_EQ(trades.size(), 3u);
    EXPECT_EQ(trades[0].getSeller(), 1u);
    EXPECT_EQ(trades[0].getPrice(), 100);
    EXPECT_EQ(trades[1].getSeller(), 2u);
    EXPECT_EQ(trades[1].getPrice(), 101);
    EXPECT_EQ(trades[2].getSeller(), 3u);
    EXPECT_EQ(trades[2].getPrice(), 102);
}

TEST_F(MatchingEngineTest, SamePriceLevelMatchesInInsertionOrder) {
    sell(1, 100, 5);
    sell(2, 100, 5);

    auto trades = buy(3, 100, 5);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].getSeller(), 1u);
    EXPECT_EQ(engine.getOrder(2).getRemainingQty(), 5u);
}

// --- Market orders ---

TEST_F(MatchingEngineTest, MarketOrderMatchesBestAvailablePrice) {
    sell(1, 105, 10);
    auto trades = marketBuy(2, 10);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].getPrice(), 105);
    EXPECT_EQ(trades[0].getQty(), 10u);
}

TEST_F(MatchingEngineTest, MarketOrderLeftoverDoesNotRest) {
    sell(1, 105, 5);
    auto trades = marketBuy(2, 10);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].getQty(), 5u);
    EXPECT_THROW(engine.getOrder(2), std::out_of_range);
}

TEST_F(MatchingEngineTest, MarketOrderWithNoLiquidityProducesNoTradesAndDoesNotRest) {
    auto trades = marketBuy(1, 10);

    EXPECT_TRUE(trades.empty());
    EXPECT_THROW(engine.getOrder(1), std::out_of_range);
}

// --- IMMEDIATE_OR_CANCEL ---

TEST_F(MatchingEngineTest, IOCWithNoMatchDoesNotRest) {
    auto trades = buy(1, 100, 10, TimeInForce::IMMEDIATE_OR_CANCEL);

    EXPECT_TRUE(trades.empty());
    EXPECT_THROW(engine.getOrder(1), std::out_of_range);
}

TEST_F(MatchingEngineTest, IOCPartialFillKeepsFillCancelsRemainder) {
    sell(1, 100, 4);
    auto trades = buy(2, 100, 10, TimeInForce::IMMEDIATE_OR_CANCEL);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].getQty(), 4u);
    EXPECT_THROW(engine.getOrder(2), std::out_of_range);
}

// --- FILL_OR_KILL ---

TEST_F(MatchingEngineTest, FOKRejectedWhenInsufficientLiquidityProducesNoTrades) {
    sell(1, 100, 4);
    auto trades = buy(2, 100, 10, TimeInForce::FILL_OR_KILL);

    EXPECT_TRUE(trades.empty());
    EXPECT_THROW(engine.getOrder(2), std::out_of_range);
    EXPECT_EQ(engine.getOrder(1).getRemainingQty(), 4u);
}

TEST_F(MatchingEngineTest, FOKAcceptedWhenLiquiditySufficientAcrossLevels) {
    sell(1, 100, 4);
    sell(2, 101, 6);

    auto trades = buy(3, 101, 10, TimeInForce::FILL_OR_KILL);

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_THROW(engine.getOrder(1), std::out_of_range);
    EXPECT_THROW(engine.getOrder(2), std::out_of_range);
    EXPECT_THROW(engine.getOrder(3), std::out_of_range);
}

TEST_F(MatchingEngineTest, FOKIgnoresLiquidityOutsidePriceLimit) {
    sell(1, 105, 10);

    auto trades = buy(2, 100, 10, TimeInForce::FILL_OR_KILL);

    EXPECT_TRUE(trades.empty());
    EXPECT_THROW(engine.getOrder(2), std::out_of_range);
}

// --- cancelOrder ---

TEST_F(MatchingEngineTest, CancelOrderRemovesRestingOrder) {
    buy(1, 100, 10);
    engine.cancelOrder(1);

    EXPECT_THROW(engine.getOrder(1), std::out_of_range);
    EXPECT_FALSE(engine.getOrderbook("AAPL").hasOrders(Side::BUY));
}

TEST_F(MatchingEngineTest, CancelOrderDoesNotAffectOtherOrders) {
    buy(1, 100, 10);
    buy(2, 99, 5);
    engine.cancelOrder(1);

    EXPECT_EQ(engine.getOrder(2).getRemainingQty(), 5u);
    EXPECT_EQ(engine.getOrderbook("AAPL").getBestOrder(Side::BUY)->getOrderId(), 2u);
}

TEST_F(MatchingEngineTest, CancelUnknownOrderThrows) {
    EXPECT_THROW(engine.cancelOrder(999), std::invalid_argument);
}

TEST_F(MatchingEngineTest, CancelAlreadyFilledOrderThrows) {
    buy(1, 100, 10);
    sell(2, 100, 10);

    EXPECT_THROW(engine.cancelOrder(1), std::invalid_argument);
}

// --- getOrder / getOrderbook ---

TEST_F(MatchingEngineTest, GetOrderThrowsForUnknownId) {
    EXPECT_THROW(engine.getOrder(42), std::out_of_range);
}

TEST_F(MatchingEngineTest, GetOrderbookThrowsForUnknownTicker) {
    EXPECT_THROW(engine.getOrderbook("NFLX"), std::out_of_range);
}

// --- STOP / STOP_LIMIT ---

TEST_F(MatchingEngineTest, StopOrderDoesNotRestOrTradeUntilTriggered) {
    buy(1, 100, 5);
    sell(2, 100, 5); // establishes last trade price = 100

    auto trades = stopBuy(3, 105, 5); // won't trigger: last (100) < stop (105)

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(engine.getOrder(3).getRemainingQty(), 5u); // still pending, untouched
    EXPECT_FALSE(engine.getOrderbook("AAPL").hasOrders(Side::BUY)); // not resting in the book
}

TEST_F(MatchingEngineTest, BuyStopTriggersAndExecutesAtMarketOnPriceRise) {
    sell(1, 110, 5); // liquidity for the stop to consume once it becomes a market order

    stopBuy(10, 105, 5); // pending; last trade price is still unset, so it can't trigger yet

    sell(2, 105, 3);
    auto trades = buy(3, 105, 3); // crosses sell(2) at 105 -> last trade price becomes 105, triggers stop(10)

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].getPrice(), 105);
    EXPECT_EQ(trades[0].getQty(), 3u);
    EXPECT_EQ(trades[1].getBuyer(), 10u);
    EXPECT_EQ(trades[1].getSeller(), 1u);
    EXPECT_EQ(trades[1].getPrice(), 110); // resting sell's price, since the stop became a market order
    EXPECT_EQ(trades[1].getQty(), 5u);

    EXPECT_THROW(engine.getOrder(10), std::out_of_range); // fully filled, removed
}

TEST_F(MatchingEngineTest, SellStopTriggersOnPriceFall) {
    buy(1, 90, 5); // liquidity for the stop to consume once it becomes a market order

    stopSell(10, 95, 5);

    buy(2, 95, 3);
    auto trades = sell(3, 95, 3); // crosses buy(2) at 95 -> last trade price becomes 95, triggers stop(10)

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[1].getSeller(), 10u);
    EXPECT_EQ(trades[1].getPrice(), 90);
    EXPECT_THROW(engine.getOrder(10), std::out_of_range);
}

TEST_F(MatchingEngineTest, StopLimitTriggersAndRestsAsLimitIfItCantFullyCross) {
    sell(1, 110, 5);

    stopLimitBuy(10, /*limitPrice=*/108, /*stopPrice=*/105, 5); // once triggered, won't pay more than 108

    sell(2, 105, 3);
    buy(3, 105, 3); // triggers stop(10) via last trade price 105

    // stop(10) is now a resting LIMIT buy at 108; sell(1) at 110 is too expensive to cross
    EXPECT_EQ(engine.getOrder(10).getRemainingQty(), 5u);
    EXPECT_EQ(engine.getOrderbook("AAPL").getBestOrder(Side::BUY)->getOrderId(), 10u);
}

TEST_F(MatchingEngineTest, StopOrderTriggersImmediatelyIfAlreadyPastTrigger) {
    sell(1, 110, 5);
    buy(2, 100, 5);
    sell(3, 100, 5); // last trade price = 100

    auto trades = stopBuy(10, 100, 5); // stop price already <= last trade price -> fires immediately

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].getBuyer(), 10u);
    EXPECT_EQ(trades[0].getSeller(), 1u);
    EXPECT_THROW(engine.getOrder(10), std::out_of_range);
}

TEST_F(MatchingEngineTest, CancelledStopOrderNeverTriggers) {
    sell(1, 110, 5);
    stopBuy(10, 100, 5);
    engine.cancelOrder(10);

    buy(2, 100, 5);
    auto trades = sell(3, 100, 5); // would have triggered stop(10) if it were still pending

    ASSERT_EQ(trades.size(), 1u); // only the buy(2)/sell(3) trade, stop never fires
    EXPECT_THROW(engine.getOrder(10), std::out_of_range);
    EXPECT_TRUE(engine.getOrderbook("AAPL").hasOrders(Side::SELL)); // sell(1) still resting, untouched
}

TEST_F(MatchingEngineTest, DuplicateIdAgainstPendingStopThrows) {
    stopBuy(1, 105, 5);
    EXPECT_THROW(buy(1, 100, 5), std::invalid_argument);
}

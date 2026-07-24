#include "Orderbook.h"
#include <gtest/gtest.h>

class OrderbookTest : public ::testing::Test {
protected:
    Order makeBuy(OrderId id, Price price, Quantity qty = 1) {
        return Order(OrderType::LIMIT, id, Side::BUY, price, qty, TimeInForce::GOOD_TILL_CANCEL);
    }
    Order makeSell(OrderId id, Price price, Quantity qty = 1) {
        return Order(OrderType::LIMIT, id, Side::SELL, price, qty, TimeInForce::GOOD_TILL_CANCEL);
    }
};

// --- Construction ---

TEST_F(OrderbookTest, OrderbookConstructorSetsAllFields) {
    Orderbook orderbook("AAPL");
    EXPECT_EQ(orderbook.getTicker(), "AAPL");
}

TEST_F(OrderbookTest, HasBuyOrdersOnEmptyBookReturnsFalse) {
    Orderbook orderbook("AAPL");
    EXPECT_FALSE(orderbook.hasOrders(Side::BUY));
}

TEST_F(OrderbookTest, HasBuyOrdersAfterAddingReturnsTrue) {
    Orderbook orderbook("AAPL");
    Order o = makeBuy(1, 100);
    orderbook.addOrder(&o);
    EXPECT_TRUE(orderbook.hasOrders(Side::BUY));
}

TEST_F(OrderbookTest, HasSellOrdersOnEmptyBookReturnsFalse) {
    Orderbook orderbook("AAPL");
    EXPECT_FALSE(orderbook.hasOrders(Side::SELL));
}

TEST_F(OrderbookTest, HasSellOrdersAfterAddingReturnsTrue) {
    Orderbook orderbook("AAPL");
    Order o = makeSell(1, 100);
    orderbook.addOrder(&o);
    EXPECT_TRUE(orderbook.hasOrders(Side::SELL));
}

// --- Single order ---

TEST_F(OrderbookTest, SingleBuyOrderIsBestBuyOrder) {
    Orderbook orderbook("AAPL");
    Order o = makeBuy(1, 100);
    orderbook.addOrder(&o);
    EXPECT_EQ(&o, orderbook.getBestOrder(Side::BUY));
}

TEST_F(OrderbookTest, SingleSellOrderIsBestSellOrder) {
    Orderbook orderbook("AAPL");
    Order o = makeSell(1, 100);
    orderbook.addOrder(&o);
    EXPECT_EQ(&o, orderbook.getBestOrder(Side::SELL));
}

TEST_F(OrderbookTest, BestBuyOrderIsHighestPriced) {
    Orderbook orderbook("AAPL");
    Order first = makeBuy(1, 100);
    Order second = makeBuy(2, 200);
    Order third = makeBuy(3, 50);

    orderbook.addOrder(&first);
    orderbook.addOrder(&second);
    orderbook.addOrder(&third);

    EXPECT_EQ(&second, orderbook.getBestOrder(Side::BUY));
}

TEST_F(OrderbookTest, BestSellOrderLowestPriced) {
    Orderbook orderbook("AAPL");
    Order first = makeSell(1, 100);
    Order second = makeSell(2, 200);
    Order third = makeSell(3, 50);

    orderbook.addOrder(&first);
    orderbook.addOrder(&second);
    orderbook.addOrder(&third);

    EXPECT_EQ(&third, orderbook.getBestOrder(Side::SELL));
}

// --- Price-time priority within a level ---

TEST_F(OrderbookTest, SamePriceBuyOrdersKeepInsertionOrder) {
    Orderbook orderbook("AAPL");
    Order first = makeBuy(1, 100);
    Order second = makeBuy(2, 100);

    orderbook.addOrder(&first);
    orderbook.addOrder(&second);

    EXPECT_EQ(&first, orderbook.getBestOrder(Side::BUY));
}

TEST_F(OrderbookTest, SamePriceSellOrdersKeepInsertionOrder) {
    Orderbook orderbook("AAPL");
    Order first = makeSell(1, 100);
    Order second = makeSell(2, 100);

    orderbook.addOrder(&first);
    orderbook.addOrder(&second);

    EXPECT_EQ(&first, orderbook.getBestOrder(Side::SELL));
}

TEST_F(OrderbookTest, OrdersMapTracksAddedOrders) {
    Orderbook orderbook("AAPL");
    Order order = makeBuy(1, 100);

    orderbook.addOrder(&order);

    ASSERT_EQ(1u, orderbook.getOrders().size());
    EXPECT_EQ(&order, orderbook.getOrders().at(1));
}

TEST_F(OrderbookTest, OrdersMapRemovesCanceledOrders) {
    Orderbook orderbook("AAPL");
    Order order = makeSell(2, 100);

    orderbook.addOrder(&order);
    orderbook.cancelOrder(2);

    EXPECT_TRUE(orderbook.getOrders().empty());
}

// --- Removal ---

TEST_F(OrderbookTest, RemoveBuyOrderRemovesIt) {
    Orderbook orderbook("AAPL");
    Order first = makeBuy(1, 50);
    Order second = makeBuy(2, 100);

    orderbook.addOrder(&first);
    orderbook.addOrder(&second);
    orderbook.removeOrder(&first);

    EXPECT_EQ(&second, orderbook.getBestOrder(Side::BUY));
}

TEST_F(OrderbookTest, RemoveSellOrderRemovesIt) {
    Orderbook orderbook("AAPL");
    Order first = makeSell(1, 200);
    Order second = makeSell(2, 100);

    orderbook.addOrder(&first);
    orderbook.addOrder(&second);
    orderbook.removeOrder(&first);

    EXPECT_EQ(&second, orderbook.getBestOrder(Side::SELL));
}

TEST_F(OrderbookTest, RemoveOneOfMultipleOrdersAtSamePriceLevel) {
    Orderbook orderbook("AAPL");
    Order first = makeBuy(1, 100);
    Order second = makeBuy(2, 100);

    orderbook.addOrder(&first);
    orderbook.addOrder(&second);
    orderbook.removeOrder(&first);

    EXPECT_EQ(&second, orderbook.getBestOrder(Side::BUY));
}
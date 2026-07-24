#pragma once

#include "OrderType.h"
#include "Types.h"
#include "Order.h"
#include "Orderbook.h"
#include "Trade.h"
#include "Constants.h"

#include <algorithm>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <stdexcept>

class MatchingEngine{
    public:
        // STOP/STOP_LIMIT orders never touch the tradable book directly: they're parked in
        // `pendingStopIds` until the market trades through their trigger, at which point
        // checkStopTriggers() converts and submits them (STOP -> MARKET, STOP_LIMIT -> LIMIT).
        // `stopPrice` is ignored for MARKET/LIMIT orders.
        std::vector<Trade> addOrder(Ticker ticker, OrderType orderType, OrderId orderId, Side side, Price price, Quantity qty, TimeInForce timeInForce, Price stopPrice = Constants::InvalidPrice){
            if(orders.contains(orderId)){
                throw std::invalid_argument("addOrder: duplicate order id");
            }

            orderbooks.try_emplace(ticker, ticker);

            std::vector<Trade> trades;

            if(orderType == OrderType::STOP || orderType == OrderType::STOP_LIMIT){
                orders.try_emplace(orderId, orderType, orderId, side, price, qty, timeInForce, stopPrice);
                orderIdToTicker[orderId] = ticker;
                pendingStopIds[ticker].push_back(orderId);
            }
            else{
                trades = submitLiveOrder(ticker, orderType, orderId, side, price, qty, timeInForce);
            }

            checkStopTriggers(ticker, trades);

            return trades;
        }

        void cancelOrder(OrderId orderId){
            auto tickerIt = orderIdToTicker.find(orderId);
            if(tickerIt == orderIdToTicker.end()){
                throw std::invalid_argument("cancelOrder: unknown order id");
            }

            Ticker ticker = tickerIt->second;
            auto& pending = pendingStopIds[ticker];
            auto pendingIt = std::find(pending.begin(), pending.end(), orderId);

            if(pendingIt != pending.end()){
                pending.erase(pendingIt);
            }
            else{
                orderbooks.at(ticker).cancelOrder(orderId);
                insertionSeq.erase(orderId);
            }

            orders.erase(orderId);
            orderIdToTicker.erase(tickerIt);
        }

        std::vector<Trade> match(Ticker ticker){
            std::vector<Trade> trades;
            Orderbook& orderbook = getOrderbook(ticker);

            while(orderbook.hasOrders(Side::BUY) && orderbook.hasOrders(Side::SELL)){
                Order* buyOrder = orderbook.getBestOrder(Side::BUY);

                if(!canMatch(orderbook, Side::BUY, buyOrder->getPrice())){
                    break;
                }

                Order* sellOrder = orderbook.getBestOrder(Side::SELL);

                Trade trade = executeTrade(buyOrder, sellOrder);
                lastTradePrice[ticker] = trade.getPrice();
                trades.push_back(trade);

                if(buyOrder->isFilled()){
                    orderbook.removeOrder(buyOrder);
                    orders.erase(buyOrder->getOrderId());
                    orderIdToTicker.erase(buyOrder->getOrderId());
                    insertionSeq.erase(buyOrder->getOrderId());
                }

                if(sellOrder->isFilled()){
                    orderbook.removeOrder(sellOrder);
                    orders.erase(sellOrder->getOrderId());
                    orderIdToTicker.erase(sellOrder->getOrderId());
                    insertionSeq.erase(sellOrder->getOrderId());
                }
            }

            return trades;
        }

        bool hasOrderbook(Ticker ticker) const{
            return orderbooks.contains(ticker);
        }

        Orderbook& getOrderbook(Ticker ticker){
            return orderbooks.at(ticker);
        }

        const Order& getOrder(OrderId orderId) const{
            return orders.at(orderId);
        }

    private:


        std::vector<Trade> submitLiveOrder(Ticker ticker, OrderType orderType, OrderId orderId, Side side, Price price, Quantity qty, TimeInForce timeInForce){
            Orderbook& orderbook = orderbooks.at(ticker);

            if(timeInForce == TimeInForce::FILL_OR_KILL && !canFillCompletely(ticker, side, price, qty)){
                return {};
            }

            auto [it, inserted] = orders.try_emplace(orderId, orderType, orderId, side, price, qty, timeInForce);
            Order& order = it->second;
            orderIdToTicker[orderId] = ticker;
            insertionSeq[orderId] = nextSeq++;

            orderbook.addOrder(&order);

            std::vector<Trade> trades = match(ticker);

            bool leftoverMustNotRest =
                (timeInForce == TimeInForce::IMMEDIATE_OR_CANCEL) ||
                (orderType == OrderType::MARKET);

            if(leftoverMustNotRest && !order.isFilled()){
                orderbook.removeOrder(&order);
                orders.erase(orderId);
                orderIdToTicker.erase(orderId);
                insertionSeq.erase(orderId);
            }

            return trades;
        }

        bool isStopTriggered(Ticker ticker, const Order& stopOrder) const{
            auto it = lastTradePrice.find(ticker);
            if(it == lastTradePrice.end()) return false; // nothing has traded yet, nothing to trigger against

            Price last = it->second;
            Price trigger = stopOrder.getStopPrice();

            // A buy-stop triggers on the way up, a sell-stop triggers on the way down.
            return (stopOrder.getSide() == Side::BUY) ? (last >= trigger) : (last <= trigger);
        }


        void checkStopTriggers(Ticker ticker, std::vector<Trade>& trades){
            while(true){
                auto& pending = pendingStopIds[ticker];
                auto triggeredIt = std::find_if(pending.begin(), pending.end(), [&](OrderId id){
                    return isStopTriggered(ticker, orders.at(id));
                });

                if(triggeredIt == pending.end()) break;

                OrderId id = *triggeredIt;
                pending.erase(triggeredIt);

                const Order& stopOrder = orders.at(id);
                Side side = stopOrder.getSide();
                Price limitPrice = stopOrder.getPrice();
                Quantity qty = stopOrder.getRemainingQty();
                TimeInForce tif = stopOrder.getTimeInForce();
                OrderType liveType = (stopOrder.getOrderType() == OrderType::STOP) ? OrderType::MARKET : OrderType::LIMIT;

                orders.erase(id);
                orderIdToTicker.erase(id);

                std::vector<Trade> cascadeTrades = submitLiveOrder(ticker, liveType, id, side, limitPrice, qty, tif);
                trades.insert(trades.end(), cascadeTrades.begin(), cascadeTrades.end());
            }
        }

        Trade executeTrade(Order* buyOrder, Order* sellOrder){
            Quantity fillQty = std::min(buyOrder->getRemainingQty(), sellOrder->getRemainingQty());

            Price tradePrice;
            if(buyOrder->getOrderType() == OrderType::MARKET){
                tradePrice = sellOrder->getPrice();
            }
            else if(sellOrder->getOrderType() == OrderType::MARKET){
                tradePrice = buyOrder->getPrice();
            }
            else{
                tradePrice = (insertionSeq.at(buyOrder->getOrderId()) <= insertionSeq.at(sellOrder->getOrderId()))
                    ? buyOrder->getPrice()
                    : sellOrder->getPrice();
            }

            buyOrder->fill(fillQty);
            sellOrder->fill(fillQty);

            return Trade(tradePrice, fillQty, buyOrder->getOrderId(), sellOrder->getOrderId());
        }

        bool canMatch(const Orderbook& orderbook, Side side, Price price) const{
            if(side == Side::BUY){
                if(!orderbook.hasOrders(Side::SELL)) return false;
                if(price == Constants::InvalidPrice) return true;

                Order* bestSellOrder = orderbook.getBestOrder(Side::SELL);
                return price >= bestSellOrder->getPrice();
            }
            else{
                if(!orderbook.hasOrders(Side::BUY)) return false;
                if(price == Constants::InvalidPrice) return true;

                Order* bestBuyOrder = orderbook.getBestOrder(Side::BUY);
                return price <= bestBuyOrder->getPrice();
            }
        }

        bool canFillCompletely(Ticker ticker, Side side, Price price, Quantity qty) const{
            if(!orderbooks.contains(ticker)) return false;

            const Orderbook& orderbook = orderbooks.at(ticker);
            bool isMarket = (price == Constants::InvalidPrice);
            Quantity available = 0;

            if(side == Side::BUY){
                for(const auto& [levelPrice, ordersAtLevel] : orderbook.getSellLevels()){
                    if(!isMarket && levelPrice > price) break;

                    for(Order* restingOrder : ordersAtLevel){
                        available += restingOrder->getRemainingQty();
                        if(available >= qty) return true;
                    }
                }
            }
            else{
                for(const auto& [levelPrice, ordersAtLevel] : orderbook.getBuyLevels()){
                    if(!isMarket && levelPrice < price) break;

                    for(Order* restingOrder : ordersAtLevel){
                        available += restingOrder->getRemainingQty();
                        if(available >= qty) return true;
                    }
                }
            }

            return available >= qty;
        }

        std::unordered_map<OrderId, Order> orders;
        std::unordered_map<Ticker, Orderbook> orderbooks;
        std::unordered_map<OrderId, Ticker> orderIdToTicker;
        std::unordered_map<OrderId, std::uint64_t> insertionSeq;
        std::uint64_t nextSeq = 0;
        std::unordered_map<Ticker, std::vector<OrderId>> pendingStopIds;
        std::unordered_map<Ticker, Price> lastTradePrice;
};

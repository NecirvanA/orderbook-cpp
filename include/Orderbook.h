#pragma once

#include "OrderType.h"
#include "Types.h"
#include "Order.h"

#include <map>
#include <list>
#include <unordered_map>

class Orderbook{
    public:
        explicit Orderbook(Ticker ticker): ticker{ticker} {}

        void addOrder(Order* order){
            addOrder(order, order->getSide());
        }

        void addOrder(Order* order, Side side){
            std::list<Order*>& level = (side == Side::BUY)
                ? buyOrders[order->getPrice()]
                : sellOrders[order->getPrice()];

            level.push_back(order);
            orderLocations[order->getOrderId()] = std::prev(level.end());

            orders[order->getOrderId()] = order;
        }

        void removeOrder(Order* order){
            removeOrder(order, order->getSide());
        }

        // Erases via the iterator captured at insertion time, so the list-erase
        // itself is O(1) regardless of how many orders share the price level;
        // the only remaining log-n cost is the std::map price-level lookup.
        void removeOrder(Order* order, Side side){
            auto locationIt = orderLocations.find(order->getOrderId());

            if(side == Side::BUY){
                auto levelIt = buyOrders.find(order->getPrice());
                levelIt->second.erase(locationIt->second);

                if(levelIt->second.empty()){
                    buyOrders.erase(levelIt);
                }
            }
            else{
                auto levelIt = sellOrders.find(order->getPrice());
                levelIt->second.erase(locationIt->second);

                if(levelIt->second.empty()){
                    sellOrders.erase(levelIt);
                }
            }

            orderLocations.erase(locationIt);
            orders.erase(order->getOrderId());
        }

        Order* getBestOrder(Side side) const {
            if(side == Side::BUY){
                return buyOrders.begin()->second.front();
            }

            return sellOrders.begin()->second.front();
        }

        void cancelOrder(OrderId orderId){
            Order* order = orders.at(orderId);
            removeOrder(order);
        }

        bool hasOrders(Side side) const {
            return side == Side::BUY ? !buyOrders.empty() : !sellOrders.empty();
        }

        const std::unordered_map<OrderId, Order*>& getOrders() const { return orders; }
        Ticker getTicker() const { return ticker; }

        // Price-sorted levels (best price first on each side), for callers that need to
        // walk the book in priority order instead of touching every resting order.
        const std::map<Price, std::list<Order*>, std::greater<int>>& getBuyLevels() const { return buyOrders; }
        const std::map<Price, std::list<Order*>>& getSellLevels() const { return sellOrders; }


    private:
        Ticker ticker;
        
        std::map<Price, std::list<Order*>, std::greater<int>> buyOrders;
        std::map<Price, std::list<Order*>> sellOrders;

        std::unordered_map<OrderId, Order*> orders;
        std::unordered_map<OrderId, std::list<Order*>::iterator> orderLocations;
};
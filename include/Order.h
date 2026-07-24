#pragma once

#include "OrderType.h"
#include "Types.h"
#include "Constants.h"

#include <stdexcept>
#include <ctime>

class Order{
    public:
        // `stopPrice` only applies to STOP/STOP_LIMIT (the trigger price); it defaults to
        // the market sentinel, meaning "no trigger", which is correct for every other type.
        Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity qty, TimeInForce timeInForce, Price stopPrice = Constants::InvalidPrice)
            : orderType{orderType}
            , orderId{orderId}
            , side{side}
            , price{price}
            , initialQty{qty}
            , remainingQty{qty}
            , timeInForce{timeInForce}
            , stopPrice{stopPrice} {
                if(qty == 0){
                    throw std::invalid_argument("Order quantity cannot be zero!");
                }

                bool priceIsSentinel = (price == Constants::InvalidPrice);
                bool stopPriceIsSentinel = (stopPrice == Constants::InvalidPrice);

                switch(orderType){
                    case OrderType::MARKET:
                        if(!priceIsSentinel){
                            throw std::invalid_argument("Market orders cannot specify a limit price!");
                        }
                        if(!stopPriceIsSentinel){
                            throw std::invalid_argument("Market orders cannot specify a stop price!");
                        }
                        break;
                    case OrderType::LIMIT:
                        if(priceIsSentinel){
                            throw std::invalid_argument("Limit orders must specify a real limit price!");
                        }
                        if(!stopPriceIsSentinel){
                            throw std::invalid_argument("Limit orders cannot specify a stop price!");
                        }
                        break;
                    case OrderType::STOP:
                        // A stop order becomes a MARKET order once triggered, so it has no
                        // limit price of its own — only the trigger (stop) price.
                        if(!priceIsSentinel){
                            throw std::invalid_argument("Stop orders cannot specify a limit price!");
                        }
                        if(stopPriceIsSentinel){
                            throw std::invalid_argument("Stop orders must specify a real stop price!");
                        }
                        break;
                    case OrderType::STOP_LIMIT:
                        if(priceIsSentinel){
                            throw std::invalid_argument("Stop-limit orders must specify a real limit price!");
                        }
                        if(stopPriceIsSentinel){
                            throw std::invalid_argument("Stop-limit orders must specify a real stop price!");
                        }
                        break;
                }

                time(&timestamp);
            }

        Order(OrderId orderId, Side side, Quantity qty, TimeInForce timeInForce) : Order(OrderType::MARKET, orderId, side, Constants::InvalidPrice, qty, timeInForce) {}

        OrderType getOrderType() const { return orderType; }
        OrderId getOrderId() const { return orderId; }
        Side getSide() const { return side; }
        Price getPrice() const { return price; }
        Price getStopPrice() const { return stopPrice; }
        Quantity getInitialQty() const { return initialQty; }
        Quantity getRemainingQty() const { return remainingQty; }
        TimeInForce getTimeInForce() const { return timeInForce; }
        time_t getTimeStamp() const { return timestamp; }
        
        bool isFilled() const { return remainingQty == 0; }

        void fill(const Quantity& qty){
            if(qty > remainingQty){
                throw std::logic_error("Cannot fill order for more than it's remaining quantity!");
            }

            remainingQty -= qty;
        }

    private:

        OrderType orderType;
        OrderId orderId;
        Side side;
        Price price;
        Quantity initialQty;
        Quantity remainingQty;
        TimeInForce timeInForce;
        Price stopPrice;
        time_t timestamp;
};
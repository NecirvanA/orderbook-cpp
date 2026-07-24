#pragma once

#include "Types.h"

class Trade{
    public:
        Trade(Price price, Quantity qty, OrderId buyer, OrderId seller) 
            : price{price}
            , qty{qty}
            , buyer{buyer}
            , seller{seller} {}

        Price getPrice() const { return price; }
        Quantity getQty() const { return qty; }
        OrderId getBuyer() const { return buyer; }
        OrderId getSeller() const { return seller; }

    private:
        Price price;
        Quantity qty;
        OrderId buyer;
        OrderId seller;
};
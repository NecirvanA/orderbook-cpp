#pragma once

enum class OrderType{
    MARKET,
    LIMIT,
    STOP,
    STOP_LIMIT
};

enum class TimeInForce{
    DAY,
    GOOD_TILL_CANCEL,
    IMMEDIATE_OR_CANCEL,
    FILL_OR_KILL,
    GOOD_TILL_DATE
};

enum class Side{
    BUY,
    SELL
};
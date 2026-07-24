#pragma once

#include <limits>
#include "Types.h"

struct Constants {
    static constexpr Price InvalidPrice = std::numeric_limits<Price>::min();
};
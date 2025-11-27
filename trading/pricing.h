#pragma once

#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {

double sale_price(stock::Symbol symbol, const trading_state& data);

}

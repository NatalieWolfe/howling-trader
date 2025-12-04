#include "data/analyzer.h"

#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

bool analyzer::can_buy(stock::Symbol symbol, const trading_state& data) const {
  return data.market.at(symbol).one_minute(-1).candle.low() <
      data.available_funds;
}

bool analyzer::can_sell(stock::Symbol symbol, const trading_state& data) const {
  auto itr = data.positions.find(symbol);
  return itr != data.positions.end() && !itr->second.empty();
}

} // namespace howling

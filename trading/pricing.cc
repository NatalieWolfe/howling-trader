#include "trading/pricing.h"

#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

double sale_price(stock::Symbol symbol, const trading_state& data) {
  const Candle& current = data.market.at(symbol).one_minute(-1).candle;
  return current.close();
}

} // namespace howling

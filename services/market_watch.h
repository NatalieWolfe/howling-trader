#pragma once

#include <span>
#include <utility>

#include "api/schwab.h"
#include "containers/buffered_stream.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"

namespace howling {

class market_watch {
public:
  explicit market_watch() : _candles{1000}, _market{1000}, _schwab{} {}
  ~market_watch();

  void start(std::span<const stock::Symbol> symbols);

  auto candle_stream() { return _candles.stream(); }
  auto market_stream() { return _market.stream(); }

private:
  buffered_stream<std::pair<stock::Symbol, Candle>> _candles;
  buffered_stream<Market> _market;
  schwab::stream _schwab;
};

} // namespace howling

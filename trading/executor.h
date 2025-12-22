#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include "api/schwab.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "trading/metrics.h"
#include "trading/trading_state.h"

namespace howling {

class executor {
public:
  explicit executor(trading_state& state);

  std::optional<trading_state::position> buy(stock::Symbol symbol, metrics& m);
  std::optional<trading_state::position> sell(stock::Symbol symbol, metrics& m);

  void update_market(const Market& market);

private:
  trading_state& _state;
  schwab::api_connection _conn;
  std::unordered_map<stock::Symbol, Market> _market;
};

} // namespace howling

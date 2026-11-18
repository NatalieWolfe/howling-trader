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
  struct enable_real_money_t {};
  struct disable_real_money_t {};
  static constexpr enable_real_money_t enable_real_money{};
  static constexpr disable_real_money_t disable_real_money{};

  explicit executor(
      trading_state& state, disable_real_money_t = disable_real_money)
      : _state{state}, _enable_real_money{false} {}
  explicit executor(trading_state& state, enable_real_money_t);

  std::optional<trading_state::position> buy(stock::Symbol symbol, metrics& m);
  std::optional<trading_state::position> sell(stock::Symbol symbol, metrics& m);

  void update_market(const Market& market);

private:
  trading_state& _state;
  schwab::api_connection _conn;
  std::unordered_map<stock::Symbol, Market> _market;
  bool _enable_real_money;
};

} // namespace howling

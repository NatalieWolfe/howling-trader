#pragma once

#include <unordered_map>

#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {

enum class action { NO_ACTION = 0, HOLD = 1, BUY = 2, SELL = 3 };

struct decision {
  action act;
  double confidence;
};

class analyzer {
public:
  virtual ~analyzer() = default;

  virtual std::unordered_map<stock::Symbol, decision>
  analyze(const trading_state& data) = 0;

  std::unordered_map<stock::Symbol, decision>
  operator()(const trading_state& data) {
    return analyze(data);
  }
};

} // namespace howling

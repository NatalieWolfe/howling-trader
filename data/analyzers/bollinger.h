#pragma once

#include <unordered_map>

#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {

class bollinger_analyzer : public analyzer {
public:
  std::unordered_map<stock::Symbol, decision>
  analyze(const trading_state& data) override;
};

} // namespace howling

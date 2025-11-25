#pragma once

#include <ranges>

#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {

class noop_analyzer : public analyzer {
public:
  decision analyze(stock::Symbol symbol, const trading_state& data) override {
    return {.act = action::NO_ACTION, .confidence = 0.0};
  }
};

} // namespace howling

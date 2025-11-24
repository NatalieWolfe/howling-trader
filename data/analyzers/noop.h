#pragma once

#include <ranges>
#include <unordered_map>

#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {

class noop_analyzer : public analyzer {
public:
  std::unordered_map<stock::Symbol, decision>
  analyze(const trading_state& data) override {
    // Issue non-actions for each held position.
    std::unordered_map<stock::Symbol, decision> decisions;
    for (const stock::Symbol symbol : data.positions | std::views::keys) {
      decisions[symbol] = {.act = action::NO_ACTION, .confidence = 0.0};
    }
    return decisions;
  }
};

} // namespace howling

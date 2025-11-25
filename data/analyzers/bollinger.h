#pragma once

#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {

class bollinger_analyzer : public analyzer {
public:
  decision analyze(stock::Symbol symbol, const trading_state& data) override;
};

} // namespace howling

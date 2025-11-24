#pragma once

#include "data/aggregate.h"

namespace howling {

enum class action { NO_ACTION = 0, HOLD = 1, BUY = 2, SELL = 3 };

struct decision {
  action act;
  double confidence;
};

class analyzer {
public:
  virtual ~analyzer() = default;

  virtual decision analyze(const aggregations& data) = 0;
  decision operator()(const aggregations& data) { return analyze(data); }
};

} // namespace howling

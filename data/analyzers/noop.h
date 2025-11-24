#pragma once

#include "data/aggregate.h"
#include "data/analyzer.h"

namespace howling {

class noop_analyzer : public analyzer {
public:
  decision analyze(const aggregations&) override {
    return {.act = action::NO_ACTION, .confidence = 0.0};
  }
};

} // namespace howling

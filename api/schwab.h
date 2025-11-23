#pragma once

#include <chrono>
#include <optional>
#include <string_view>
#include <vector>

#include "containers/vector.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"

namespace howling::schwab {

struct get_history_parameters {
  std::string_view period_type = "day";
  int period = 1;
  std::string_view frequency_type = "minute";
  int frequency = 1;
  std::optional<std::chrono::system_clock::time_point> start_date;
  std::optional<std::chrono::system_clock::time_point> end_date;
  bool need_extended_hours_data = true;
  bool need_previous_close = false;
};

vector<Candle>
get_history(stock::Symbol symbol, const get_history_parameters& params);

} // namespace howling::schwab

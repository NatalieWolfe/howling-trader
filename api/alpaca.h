#pragma once

#include <chrono>
#include <optional>
#include <string_view>

#include "containers/vector.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"

namespace howling::alpaca {

struct get_stock_bars_parameters {
  std::string_view timeframe = "1Min";
  std::optional<std::chrono::system_clock::time_point> start;
  std::optional<std::chrono::system_clock::time_point> end;
  int limit = 1000;
  std::string_view sort = "asc";
  std::string_view adjustment = "raw";
  std::string_view feed = "iex"; // TODO: Change to "sip" once subscribed.
  std::string_view currency = "USD";
  std::optional<std::string_view> page_token;
};

vector<Candle>
get_stock_bars(stock::Symbol symbol, get_stock_bars_parameters params = {});

} // namespace howling::alpaca

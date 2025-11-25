#pragma once

#include <vector>

#include "containers/vector.h"
#include "data/candle.pb.h"

namespace howling {

struct window {
  Candle candle;

  bool green_body;
  double body_high;
  double body_low;
  double price_delta;

  double upper_wick_length;
  double lower_wick_length;
  double total_wick_length;
  double wick_body_ratio;

  double moving_average;
  double stddev;

  double upper_bollinger_band;
  double lower_bollinger_band;

  bool green_sequence;
  int setup_counter;
  int countdown_counter;
};

/**
 * Moving data aggregations over different window sizes.
 *
 * All lists of aggregations step forward by 1 minute for each contained window.
 */
struct aggregations {
  vector<window> one_minute;
  vector<window> five_minute;
  vector<window> twenty_minute;
};

aggregations aggregate(const vector<Candle>& one_minute_candles);

void add_next_minute(aggregations& aggr, const Candle& candle);

} // namespace howling

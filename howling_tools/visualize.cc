#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <limits>
#include <string>

#include "absl/flags/flag.h"
#include "cli/printing.h"
#include "data/analyzer.h"
#include "data/candle.pb.h"
#include "data/load_analyzer.h"
#include "data/stock.pb.h"
#include "data/utilities.h"
#include "environment/init.h"
#include "environment/runfiles.h"
#include "time/conversion.h"
#include "trading/metrics.h"
#include "trading/trading_state.h"

ABSL_FLAG(
    howling::stock::Symbol,
    stock,
    howling::stock::SYMBOL_UNSPECIFIED,
    "Stock symbol to visualize.");
ABSL_FLAG(std::string, date, "", "Day to visualize.");
ABSL_FLAG(std::string, analyzer, "", "Name of an analyzer to run.");
ABSL_FLAG(
    double,
    initial_funds,
    200'000,
    "Available funds at begining of period to visualize.");

namespace howling {
namespace {

using ::std::chrono::system_clock;

void run() {
  howling::stock::Symbol symbol = absl::GetFlag(FLAGS_stock);
  if (symbol == stock::SYMBOL_UNSPECIFIED) {
    throw std::runtime_error("Must specify a stock symbol.");
  }
  stock::History history = read_history(runfile(
      get_history_file_path(symbol, absl::GetFlag(FLAGS_date)).string()));
  auto anal = load_analyzer(absl::GetFlag(FLAGS_analyzer), history);

  print_candle_parameters print_params{
      .price_min = std::numeric_limits<double>::max(),
      .price_max = std::numeric_limits<double>::min(),
      .candle_print_min = std::numeric_limits<double>::max(),
      .candle_print_max = std::numeric_limits<double>::min(),
      .candle_width = 0.70};
  for (const Candle& candle : history.candles()) {
    print_params.price_min = print_params.candle_print_min =
        std::min(print_params.candle_print_min, candle.low());
    print_params.price_max = print_params.candle_print_max =
        std::max(print_params.candle_print_max, candle.high());
  }

  trading_state state{
      .available_stocks = vector<stock::Symbol>{{symbol}},
      .initial_funds = absl::GetFlag(FLAGS_initial_funds),
      .available_funds = absl::GetFlag(FLAGS_initial_funds)};
  metrics m{.name = "Summary", .initial_funds = state.initial_funds};
  for (const Candle& candle : history.candles()) {
    state.time_now =
        to_std_chrono(candle.opened_at()) + to_std_chrono(candle.duration());
    add_next_minute(state.market[symbol], candle);
    decision d = anal->analyze(symbol, state);

    std::cout << print_candle(d, /*trade=*/std::nullopt, candle, print_params)
              << "\n";

    // TODO: Support quantities and target prices in buy and sell decisions.
    if (d.act == action::BUY) {
      print_params.last_buy_price = candle.close();
      state.available_funds -= candle.close();
      state.positions[symbol].push_back(
          {.symbol = symbol, .price = candle.close(), .quantity = 1});
    } else if (d.act == action::SELL && !state.positions[symbol].empty()) {
      ++m.sales;
      state.available_funds += candle.close();
      double last_buy = state.positions[symbol].back().price;
      if (last_buy < candle.close()) ++m.profitable_sales;
      state.positions[symbol].pop_back();
    }
  }
  m.available_funds = state.available_funds;
  m.assets_value = state.total_positions_value();
  if (m.sales > 0 || m.assets_value > 0) {
    std::cout << "\n" << print_metrics(m) << "\n";
  }
}

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  howling::init(argc, argv);

  try {
    howling::run();
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  } catch (...) { std::cerr << "!!!! UNKNOWN ERROR THROWN !!!!" << std::endl; }
  return 1;
}

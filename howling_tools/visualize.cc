#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <limits>
#include <string>

#include "absl/flags/flag.h"
#include "cli/colorize.h"
#include "cli/printing.h"
#include "data/analyzer.h"
#include "data/candle.pb.h"
#include "data/load_analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"
#include "data/utilities.h"
#include "howling_tools/init.h"
#include "howling_tools/runfiles.h"
#include "time/conversion.h"

ABSL_FLAG(std::string, stock, "", "Stock symbol to visualize.");
ABSL_FLAG(std::string, date, "", "Day to visualize.");
ABSL_FLAG(std::string, analyzer, "", "Name of an analyzer to run.");
ABSL_FLAG(
    double,
    initial_funds,
    200'000,
    "Available funds at begining of period to visualize.");

namespace howling {
namespace {

using ::std::chrono::hh_mm_ss;
using ::std::chrono::system_clock;

void run() {
  stock::Symbol symbol = get_stock_symbol(absl::GetFlag(FLAGS_stock));
  stock::History history = read_history(runfile(
      get_history_file_path(symbol, absl::GetFlag(FLAGS_date)).string()));
  auto anal = load_analyzer(absl::GetFlag(FLAGS_analyzer), history);

  print_extents extents{
      .min = std::numeric_limits<double>::max(),
      .max = std::numeric_limits<double>::min(),
      .max_width_usage = 0.75};
  double min_close = std::numeric_limits<double>::max();
  double max_close = std::numeric_limits<double>::min();
  for (const Candle& candle : history.candles()) {
    extents.min = std::min(extents.min, candle.low());
    extents.max = std::max(extents.max, candle.high());
    min_close = std::min(min_close, candle.close());
    max_close = std::max(max_close, candle.close());
  }

  trading_state state{
      .available_stocks = vector<stock::Symbol>{{symbol}},
      .initial_funds = absl::GetFlag(FLAGS_initial_funds),
      .available_funds = absl::GetFlag(FLAGS_initial_funds)};
  int buy_counter = 0;
  int sell_counter = 0;
  int profitable_trades = 0;
  for (const Candle& candle : history.candles()) {
    using namespace ::std::chrono;
    zoned_time opened_at{current_zone(), to_std_chrono(candle.opened_at())};
    hh_mm_ss time_of_day{floor<seconds>(
        opened_at.get_local_time() - floor<days>(opened_at.get_local_time()))};
    const bool is_ref_point = time_of_day.minutes().count() % 15 == 0;
    if (is_ref_point) {
      std::cout << ' ' << time_of_day << " | ";
    } else {
      std::cout << "          | ";
    }
    std::cout << print_candle(candle, extents) << " | ";

    state.time_now =
        to_std_chrono(candle.opened_at()) + to_std_chrono(candle.duration());
    add_next_minute(state.market[symbol], candle);
    decision d = anal->analyze(symbol, state);
    // TODO: Support quantities and target prices in buy and sell decisions.
    if (d.act == action::BUY) {
      ++buy_counter;
      state.available_funds -= candle.close();
      state.positions[symbol].push_back(
          {.symbol = symbol, .price = candle.close(), .quantity = 1});
      std::cout << colorize(print_price(candle.close()), color::RED)
                << " - Buy (" << d.confidence << ")";
    } else if (d.act == action::SELL && !state.positions[symbol].empty()) {
      ++sell_counter;
      state.available_funds += candle.close();
      double last_buy = state.positions[symbol].back().price;
      if (last_buy < candle.close()) ++profitable_trades;
      state.positions[symbol].pop_back();
      std::cout << colorize(print_price(candle.close()), color::GREEN)
                << " - Sell (" << std::setprecision(2) << d.confidence << "; "
                << print_price(candle.close() - last_buy) << ")";
    } else if (candle.close() == min_close) {
      std::cout << colorize(print_price(candle.close()), color::RED);
    } else if (candle.close() == max_close) {
      std::cout << colorize(print_price(candle.close()), color::GREEN);
    } else if (is_ref_point) {
      std::cout << colorize(print_price(candle.close()), color::GRAY);
    }
    std::cout << '\n';
  }
  if (buy_counter > 0) {
    double profit = state.available_funds + state.total_positions_value() -
        state.initial_funds;
    std::cout << "\n"
              << "Buys:    " << buy_counter << "\n"
              << "Sells:   " << sell_counter << "\n"
              << "+Trades: " << profitable_trades << "\n"
              << "Profit:  "
              << colorize(
                     print_price(profit),
                     profit > 0 ? color::GREEN : color::RED)
              << "\n";
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

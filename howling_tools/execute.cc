#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <ranges>
#include <thread>

#include "absl/flags/flag.h"
#include "api/schwab.h"
#include "cli/printing.h"
#include "data/candle.pb.h"
#include "data/load_analyzer.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "data/utilities.h"
#include "howling_tools/init.h"
#include "time/conversion.h"
#include "trading/executor.h"
#include "trading/trading_state.h"

ABSL_FLAG(std::string, stock, "", "Stock symbol to evaluate against.");
ABSL_FLAG(std::string, analyzer, "howling", "Name of an analyzer to evaluate.");

namespace howling {
namespace {

using namespace ::std::chrono_literals;

using ::std::chrono::floor;
using ::std::chrono::minutes;
using ::std::chrono::system_clock;

class execution_printer {
public:
  execution_printer() {}

  void print(const Candle& candle, const decision& d) {
    _clear_line();
    if (_update_limits(candle)) {
      std::cout << "\nPrint bounds " << _extents.min << "-" << _extents.max
                << "\n\n";
    }
    std::cout << print_candle(d, _metrics, candle, _extents) << "\n";
    _print_length = 0;
    _reset_current_minute();
  }

  void print(const Market& market) {
    if (market.last() == 0.0) return;

    if (_current_minute.open() == 0.0) {
      _current_minute.set_open(market.last());
      *_current_minute.mutable_opened_at() = to_proto(
          std::chrono::floor<std::chrono::minutes>(
              to_std_chrono(market.emitted_at())));
    }
    _current_minute.set_close(market.last());
    _current_minute.set_low(std::min(_current_minute.low(), market.last()));
    _current_minute.set_high(std::max(_current_minute.high(), market.last()));
    *_current_minute.mutable_duration() = to_proto(
        to_std_chrono(market.emitted_at()) -
        to_std_chrono(_current_minute.opened_at()));

    _clear_line();
    _update_limits(_current_minute);
    std::string line =
        print_candle(NO_ACTION, _metrics, _current_minute, _extents);
    _print_length = 165; // line.length();
    std::cout << line << std::flush;
  }

private:
  void _clear_line() {
    if (_print_length > 0) {
      std::cout << '\r' << std::string(_print_length, ' ') << '\r';
    }
    _print_length = 0;
  }

  void _reset_current_minute() {
    _current_minute.Clear();
    _current_minute.set_low(std::numeric_limits<double>::max());
    _current_minute.set_high(std::numeric_limits<double>::min());
  }

  bool _update_limits(const Candle& candle) {
    _metrics.min = std::min(_metrics.min, candle.low());
    _metrics.max = std::max(_metrics.max, candle.high());
    if (candle.low() < _extents.min || candle.high() > _extents.max) {
      _extents.min = std::min(_extents.min, candle.low() - 0.1);
      _extents.max = std::max(_extents.max, candle.high() + 0.1);
      return true;
    }
    return false;
  }

  // TODO: Merge metrics min/max into print_extents
  print_extents _extents{
      .min = std::numeric_limits<double>::max(),
      .max = std::numeric_limits<double>::min(),
      .max_width_usage = 0.75};
  metrics _metrics;
  Candle _current_minute;
  int _print_length = 0;
};

void run() {
  if (absl::GetFlag(FLAGS_analyzer).empty()) {
    throw std::runtime_error("Must specify an analyzer.");
  }
  stock::Symbol symbol = get_stock_symbol(absl::GetFlag(FLAGS_stock));
  auto anal = load_analyzer(absl::GetFlag(FLAGS_analyzer));

  // TODO: Fetch available funds from Schwab.
  trading_state state{
      .available_stocks = {{symbol}},
      .initial_funds = 20'000,
      .available_funds = 20'000};
  metrics m{.name = "Summary", .initial_funds = state.initial_funds};
  executor e{state};
  execution_printer printer;

  for (const Candle& candle : schwab::get_history(
           symbol, {.end_date = std::chrono::system_clock::now()})) {
    state.time_now =
        to_std_chrono(candle.opened_at()) + to_std_chrono(candle.duration());
    add_next_minute(state.market[symbol], candle);
    printer.print(candle, NO_ACTION);
  }

  schwab::stream stream;
  std::thread runner{[&]() { stream.start(); }};

  stream.on_chart([&](stock::Symbol symbol, Candle candle) {
    system_clock::duration candle_duration = to_std_chrono(candle.duration());
    if (candle_duration != 60s) {
      throw std::runtime_error("Unexpected candle duration received!");
    }
    state.time_now = to_std_chrono(candle.opened_at()) + candle_duration;
    add_next_minute(state.market[symbol], candle);
    decision d = anal->analyze(symbol, state);

    printer.print(candle, d);

    if (d.act == action::BUY) {
      e.buy(symbol, m);
    } else if (d.act == action::SELL) {
      e.sell(symbol, m);
    }
  });

  stream.on_market([&](stock::Symbol symbol, Market market) {
    printer.print(market);
    e.update_market(std::move(market));
  });

  while (!stream.is_running()) std::this_thread::sleep_for(1ms);
  stream.add_symbol(symbol);

  // Wait until after market close to shutdown.
  if (state.market_hour() < 15) {
    std::this_thread::sleep_for(std::chrono::hours(15 - state.market_hour()));
  }
  while (state.market_is_open()) std::this_thread::sleep_for(1min);
  stream.stop();
  runner.join();

  std::cout << "\n" << print_metrics(m) << std::endl;
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

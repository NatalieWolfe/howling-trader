#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <ranges>
#include <thread>

#include "absl/flags/flag.h"
#include "api/schwab.h"
#include "cli/printing.h"
#include "containers/vector.h"
#include "data/account.pb.h"
#include "data/candle.pb.h"
#include "data/load_analyzer.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "data/utilities.h"
#include "howling_tools/init.h"
#include "services/market_watch.h"
#include "time/conversion.h"
#include "trading/executor.h"
#include "trading/trading_state.h"

ABSL_FLAG(std::string, stock, "", "Stock symbol to evaluate against.");
ABSL_FLAG(std::string, analyzer, "howling", "Name of an analyzer to evaluate.");
ABSL_FLAG(std::string, account, "", "Name of account to use for trading.");

namespace howling {
namespace {

using namespace ::std::chrono_literals;

using ::std::chrono::floor;
using ::std::chrono::minutes;
using ::std::chrono::system_clock;

class execution_printer {
public:
  execution_printer() {}

  void print(
      const Candle& candle,
      const decision& d,
      const std::optional<trading_state::position>& trade) {
    _clear_line();
    if (_update_limits(candle)) {
      std::cout << "\nPrint bounds " << _params.candle_print_min << "-"
                << _params.candle_print_max << "\n\n";
    }
    if (d.act == action::BUY && trade) _params.last_buy_price = trade->price;
    std::cout << print_candle(d, trade, candle, _params) << "\n";
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
        print_candle(NO_ACTION, std::nullopt, _current_minute, _params);
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
    _params.price_min = std::min(_params.price_min, candle.low());
    _params.price_max = std::max(_params.price_max, candle.high());
    if (candle.low() < _params.candle_print_min ||
        candle.high() > _params.candle_print_max) {
      _params.candle_print_min =
          std::min(_params.candle_print_min, candle.low() - 0.1);
      _params.candle_print_max =
          std::max(_params.candle_print_max, candle.high() + 0.1);
      return true;
    }
    return false;
  }

  print_candle_parameters _params{
      .price_min = std::numeric_limits<double>::max(),
      .price_max = std::numeric_limits<double>::min(),
      .candle_print_min = std::numeric_limits<double>::max(),
      .candle_print_max = std::numeric_limits<double>::min(),
      .candle_width = 0.70};
  Candle _current_minute;
  int _print_length = 0;
};

Account get_account(schwab::api_connection& api) {
  for (const Account& account : api.get_accounts()) {
    if (account.name() == absl::GetFlag(FLAGS_account)) return account;
  }
  throw std::runtime_error(
      "No account found with name " + absl::GetFlag(FLAGS_account));
}

void load_positions(
    schwab::api_connection& api,
    trading_state& state,
    std::string_view account_id) {
  for (const stock::Position& position :
       api.get_account_positions(account_id)) {
    state.positions[position.symbol()].push_back(
        {.symbol = position.symbol(),
         .price = position.price(),
         .quantity = position.quantity()});
  }
}

trading_state load_trading_state(vector<stock::Symbol> symbols) {
  // TODO: Use account.available_funds for initial trading state.
  schwab::api_connection api;
  Account account = get_account(api);
  trading_state state{
      .available_stocks = std::move(symbols),
      .account_id = account.account_id(),
      .initial_funds = 20'000,
      .available_funds = 20'000};
  load_positions(api, state, account.account_id());

  return state;
}

void run() {
  if (absl::GetFlag(FLAGS_analyzer).empty()) {
    throw std::runtime_error("Must specify an analyzer.");
  }
  vector<stock::Symbol> symbols;
  symbols.push_back(get_stock_symbol(absl::GetFlag(FLAGS_stock)));
  auto anal = load_analyzer(absl::GetFlag(FLAGS_analyzer));

  execution_printer printer;
  trading_state state = load_trading_state(std::move(symbols));
  metrics m{.name = "Summary", .initial_funds = state.initial_funds};
  executor e{state};

  auto watcher = std::make_unique<market_watch>();
  std::thread candle_streamer([&]() {
    for (const auto& [symbol, candle] : watcher->candle_stream()) {
      system_clock::duration candle_duration = to_std_chrono(candle.duration());
      if (candle_duration != 60s) {
        throw std::runtime_error("Unexpected candle duration received!");
      }
      state.time_now = to_std_chrono(candle.opened_at()) + candle_duration;
      add_next_minute(state.market[symbol], candle);
      decision d = anal->analyze(symbol, state);

      std::optional<trading_state::position> trade = std::nullopt;
      if (d.act == action::BUY) {
        trade = e.buy(symbol, m);
      } else if (d.act == action::SELL) {
        trade = e.sell(symbol, m);
      }

      printer.print(candle, d, trade);
    }
  });

  std::thread market_streamer([&]() {
    for (const Market& market : watcher->market_stream()) {
      printer.print(market);
      e.update_market(std::move(market));
    }
  });

  std::thread watcher_thread([&]() { watcher->start(state.available_stocks); });

  // Wait for the trading state to catch up with now.
  while (system_clock::now() - state.time_now > 2min) {
    std::this_thread::sleep_for(1s);
  }

  // Wait until after market close to shutdown.
  if (state.market_hour() < 15) {
    std::this_thread::sleep_for(std::chrono::hours(15 - state.market_hour()));
  }
  while (state.market_hour() == 15 && state.market_minute() < 45) {
    std::this_thread::sleep_for(1min);
  }
  watcher = nullptr;
  watcher_thread.join();

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

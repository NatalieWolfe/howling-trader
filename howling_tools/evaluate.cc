#include <utility>
#include <chrono>
#include <exception>
#include <filesystem>
#include <format>
#include <generator>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>

#include "absl/flags/flag.h"
#include "cli/printing.h"
#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/load_analyzer.h"
#include "data/stock.pb.h"
#include "data/utilities.h"
#include "environment/init.h"
#include "environment/runfiles.h"
#include "services/database.h"
#include "services/db/make_database.h"
#include "time/conversion.h"
#include "trading/metrics.h"
#include "trading/trading_state.h"

ABSL_FLAG(
    howling::stock::Symbol,
    stock,
    howling::stock::SYMBOL_UNSPECIFIED,
    "Stock symbol to evaluate against.");
ABSL_FLAG(std::string, analyzer, "", "Name of an analyzer to evaluate.");
ABSL_FLAG(
    double,
    initial_funds,
    200'000,
    "Available funds at begining of evaluation.");
ABSL_FLAG(bool, use_database, false, "Use database for evaluation.");

namespace howling {
namespace {

namespace fs = ::std::filesystem;

using ::std::chrono::system_clock;
using ::std::chrono::year_month_day;

year_month_day parse_date(std::string date) {
  year_month_day ymd;
  std::stringstream stream{std::move(date)};
  std::chrono::from_stream(stream, "%F", ymd);
  return ymd;
}

year_month_day get_date(system_clock::time_point time) {
  return year_month_day{std::chrono::floor<std::chrono::days>(time)};
}

std::string print_date(year_month_day ymd) {
  return std::format("{:%B %Y}", ymd);
}

struct day_data {
  std::string name;
  vector<Candle> candles;
};

std::generator<day_data> get_days(stock::Symbol symbol) {
  if (absl::GetFlag(FLAGS_use_database)) {
    auto db = make_database();
    vector<Candle> day_candles;
    std::string day_name;
    for (const Candle& candle : db->read_candles(symbol)) {
      std::string current_day = std::format(
          "{:%F}",
          std::chrono::floor<std::chrono::days>(
              to_std_chrono(candle.opened_at())));
      if (day_candles.empty()) {
        day_name = current_day;
      } else if (current_day != day_name) {
        co_yield {day_name, std::move(day_candles)};
        day_candles.clear();
        day_name = current_day;
      }
      day_candles.push_back(candle);
    }
    if (!day_candles.empty()) co_yield {day_name, std::move(day_candles)};
  } else {
    fs::path data_directory = runfile(
        get_history_file_path(symbol, /*date=*/"").parent_path().string());
    auto files = fs::directory_iterator(data_directory) |
        std::ranges::to<vector<fs::directory_entry>>();
    std::ranges::sort(
        files, [](const fs::directory_entry& a, const fs::directory_entry& b) {
          return std::ranges::lexicographical_compare(
              a.path().filename(), b.path().filename());
        });
    for (const fs::directory_entry& file : files) {
      stock::History history = read_history(file.path());
      vector<Candle> day_candles;
      day_candles.reserve(history.candles_size());
      for (const Candle& c : history.candles()) day_candles.push_back(c);
      co_yield {file.path().stem().string(), std::move(day_candles)};
    }
  }
}

void run() {
  if (absl::GetFlag(FLAGS_analyzer).empty()) {
    throw std::runtime_error("Must specify an analyzer.");
  }
  stock::Symbol symbol = absl::GetFlag(FLAGS_stock);
  if (symbol == stock::SYMBOL_UNSPECIFIED) {
    throw std::runtime_error("Must specify a stock symbol.");
  }
  auto anal = load_analyzer(absl::GetFlag(FLAGS_analyzer));

  trading_state state{
      .available_stocks = vector<stock::Symbol>{{symbol}},
      .initial_funds = absl::GetFlag(FLAGS_initial_funds),
      .available_funds = absl::GetFlag(FLAGS_initial_funds)};

  vector<metrics> months;
  year_month_day previous_date;
  bool first_day = true;

  for (day_data day : get_days(symbol)) {
    if (first_day) {
      previous_date = parse_date(day.name);
      months.push_back(
          {.name = print_date(previous_date),
           .initial_funds = state.initial_funds});
      first_day = false;
    }

    state.time_now = to_std_chrono(day.candles.front().opened_at());
    year_month_day current_date = get_date(state.time_now);
    if (current_date.month() != previous_date.month()) {
      months.back().assets_value = state.total_positions_value();
      months.push_back(
          {.name = print_date(current_date),
           .initial_funds = state.available_funds});
      previous_date = current_date;
    }
    metrics day_metrics{
        .name = day.name, .initial_funds = state.available_funds};

    for (const Candle& candle : day.candles) {
      state.time_now =
          to_std_chrono(candle.opened_at()) + to_std_chrono(candle.duration());

      add_next_minute(state.market[symbol], candle);
      decision d = anal->analyze(symbol, state);
      if (d.act == action::BUY) {
        state.available_funds -= candle.close();
        state.positions[symbol].push_back(
            {.symbol = symbol, .price = candle.close(), .quantity = 1});
      } else if (d.act == action::SELL && !state.positions[symbol].empty()) {
        for (const trading_state::position& p : state.positions[symbol]) {
          ++day_metrics.sales;
          if (p.price < candle.close()) ++day_metrics.profitable_sales;
          state.available_funds += candle.close() * p.quantity;
        }
        state.positions[symbol].clear();
      }
    }
    day_metrics.available_funds = state.available_funds;
    day_metrics.assets_value = state.total_positions_value();
    std::cout << print_metrics(day_metrics) << "\n";
    add_metrics(months.back(), day_metrics);
  }

  if (months.empty()) return;

  metrics totals{.name = "Total", .initial_funds = state.initial_funds};
  std::cout << "\n";
  for (const metrics& m : months) {
    std::cout << print_metrics(m) << "\n";
    add_metrics(totals, m);
  }
  totals.assets_value = state.total_positions_value();
  std::cout << "\n" << print_metrics(totals) << "\n";
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

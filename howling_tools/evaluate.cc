#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>

#include "absl/flags/flag.h"
#include "cli/colorize.h"
#include "cli/printing.h"
#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/load_analyzer.h"
#include "data/stock.pb.h"
#include "data/utilities.h"
#include "howling_tools/init.h"
#include "howling_tools/runfiles.h"
#include "time/conversion.h"

ABSL_FLAG(std::string, stock, "", "Stock symbol to evaluate against.");
ABSL_FLAG(std::string, analyzer, "", "Name of an analyzer to evaluate.");
ABSL_FLAG(
    double,
    initial_funds,
    200'000,
    "Available funds at begining of evaluation.");

namespace howling {
namespace {

namespace fs = ::std::filesystem;

using ::std::chrono::system_clock;
using ::std::chrono::year_month_day;

struct metrics {
  std::string name;
  double initial_funds;
  double available_funds = initial_funds;
  double assets_value = 0;
  int sales = 0;
  int profitable_sales = 0;
};

year_month_day parse_date(std::string date) {
  year_month_day ymd;
  std::stringstream stream{std::move(date)};
  std::chrono::from_stream(stream, "%Y-%m-%d", ymd);
  return ymd;
}

year_month_day get_date(system_clock::time_point time) {
  return year_month_day{std::chrono::floor<std::chrono::days>(time)};
}

std::string print_date(year_month_day ymd) {
  return std::format("{:%B %Y}", ymd);
}

void add_metrics(metrics& lhs, const metrics& rhs) {
  lhs.available_funds = rhs.available_funds;
  lhs.sales += rhs.sales;
  lhs.profitable_sales += rhs.profitable_sales;
}

std::string print_metrics(const metrics& m) {
  double profit = m.available_funds + m.assets_value - m.initial_funds;
  return absl::StrCat(
      m.name,
      "\n  Sales:   ",
      m.sales,
      "\n  +Sales:  ",
      m.profitable_sales,
      "\n  $ Delta: ",
      colorize(
          print_price(profit),
          profit > 0.0       ? color::GREEN
              : profit < 0.0 ? color::RED
                             : color::GRAY));
}

void run() {
  if (absl::GetFlag(FLAGS_analyzer).empty()) {
    throw std::runtime_error("Must specify an analyzer.");
  }
  stock::Symbol symbol = get_stock_symbol(absl::GetFlag(FLAGS_stock));
  auto anal = load_analyzer(absl::GetFlag(FLAGS_analyzer));

  fs::path data_directory = runfile(
      get_history_file_path(symbol, /*date=*/"").parent_path().string());
  auto files = fs::directory_iterator(data_directory) |
      std::ranges::to<vector<fs::directory_entry>>();
  std::ranges::sort(
      files, [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return std::ranges::lexicographical_compare(
            a.path().filename(), b.path().filename());
      });

  trading_state state{
      .available_stocks = vector<stock::Symbol>{{symbol}},
      .initial_funds = absl::GetFlag(FLAGS_initial_funds),
      .available_funds = absl::GetFlag(FLAGS_initial_funds)};

  year_month_day previous_date = parse_date(files.front().path().stem());
  std::vector<metrics> months{
      {.name = print_date(previous_date),
       .initial_funds = state.initial_funds}};
  for (const fs::directory_entry& file : files) {
    stock::History history = read_history(file.path());
    state.time_now = to_std_chrono(history.candles(0).opened_at());
    year_month_day current_date = get_date(state.time_now);
    if (current_date.month() != previous_date.month()) {
      months.back().assets_value = state.total_positions_value();
      months.push_back(
          {.name = print_date(current_date),
           .initial_funds = state.available_funds});
      previous_date = current_date;
    }
    metrics day{
        .name = file.path().stem().string(),
        .initial_funds = state.available_funds};

    for (const Candle& candle : history.candles()) {
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
          ++day.sales;
          if (p.price < candle.close()) ++day.profitable_sales;
          state.available_funds += candle.close() * p.quantity;
        }
        state.positions[symbol].clear();
      }
    }
    day.available_funds = state.available_funds;
    day.assets_value = state.total_positions_value();
    std::cout << print_metrics(day) << "\n";
    add_metrics(months.back(), day);
  }
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

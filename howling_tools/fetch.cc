#include <chrono>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "api/alpaca.h"
#include "api/schwab.h"
#include "containers/vector.h"
#include "data/stock.pb.h"
#include "data/utilities.h"
#include "environment/init.h"
#include "google/protobuf/text_format.h"
#include "time/conversion.h"

ABSL_FLAG(
    howling::stock::Symbol,
    stock,
    howling::stock::SYMBOL_UNSPECIFIED,
    "Stock symbol to fetch.");
ABSL_FLAG(
    absl::Duration,
    duration,
    absl::Minutes(60),
    "The length of window of stock history to fetch.");
ABSL_FLAG(
    absl::Time,
    start,
    absl::UniversalEpoch(),
    "Oldest data to fetch. Default is `duration` before now.");
ABSL_FLAG(
    std::string,
    api,
    "schwab",
    "The API to use for fetching stocks. Either 'alpaca' or 'schwab'");

namespace howling {
namespace {

using ::google::protobuf::TextFormat;
using ::std::chrono::system_clock;

bool has_started_at() {
  return absl::GetFlag(FLAGS_start) != absl::UniversalEpoch();
}

system_clock::time_point get_started_at() {
  if (!has_started_at()) {
    return system_clock::now() - to_std_chrono(absl::GetFlag(FLAGS_duration));
  }
  return to_std_chrono(absl::GetFlag(FLAGS_start));
}

void run_alpaca() {
  howling::stock::Symbol symbol = absl::GetFlag(FLAGS_stock);
  if (symbol == stock::SYMBOL_UNSPECIFIED) {
    throw std::runtime_error("Must specify a stock symbol.");
  }
  system_clock::time_point started_at = get_started_at();
  std::optional<system_clock::time_point> ended_at;
  if (has_started_at()) {
    ended_at = started_at + to_std_chrono(absl::GetFlag(FLAGS_duration)) -
        std::chrono::milliseconds(1);
  }

  stock::History history;
  history.set_symbol(symbol);
  *history.mutable_started_at() = to_proto(started_at);
  *history.mutable_duration() = to_proto(absl::GetFlag(FLAGS_duration));

  vector<Candle> candles =
      alpaca::get_stock_bars(symbol, {.start = started_at, .end = ended_at});
  history.mutable_candles()->Reserve(candles.size());
  history.mutable_candles()->Assign(candles.begin(), candles.end());

  std::string buffer;
  if (!TextFormat::PrintToString(history, &buffer)) {
    throw std::runtime_error("Failed to format history for printing.");
  }
  std::cout << buffer << std::endl;
}

void run_schwab() {
  howling::stock::Symbol symbol = absl::GetFlag(FLAGS_stock);
  if (symbol == stock::SYMBOL_UNSPECIFIED) {
    throw std::runtime_error("Must specify a stock symbol.");
  }
  system_clock::time_point started_at = get_started_at();
  std::optional<system_clock::time_point> ended_at;
  if (has_started_at()) {
    ended_at = started_at + to_std_chrono(absl::GetFlag(FLAGS_duration)) -
        std::chrono::milliseconds(1);
  }

  stock::History history;
  history.set_symbol(symbol);
  *history.mutable_started_at() = to_proto(started_at);
  *history.mutable_duration() = to_proto(absl::GetFlag(FLAGS_duration));

  vector<Candle> candles = schwab::api_connection{}.get_history(
      symbol, {.start_date = started_at, .end_date = ended_at});
  history.mutable_candles()->Reserve(candles.size());
  history.mutable_candles()->Assign(candles.begin(), candles.end());

  std::string buffer;
  if (!TextFormat::PrintToString(history, &buffer)) {
    throw std::runtime_error("Failed to format history for printing.");
  }
  std::cout << buffer << std::endl;
}

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  howling::init(argc, argv);

  try {
    if (absl::GetFlag(FLAGS_api) == "alpaca") {
      howling::run_alpaca();
    } else if (absl::GetFlag(FLAGS_api) == "schwab") {
      howling::run_schwab();
    } else {
      throw std::runtime_error(
          absl::StrCat("Unknown api: ", absl::GetFlag(FLAGS_api)));
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  } catch (...) { std::cerr << "!!!! UNKNOWN ERROR THROWN !!!!" << std::endl; }
  return 1;
}

#include <cctype>
#include <chrono>
#include <exception>
#include <iostream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/log/log_sink_registry.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "api/alpaca.h"
#include "api/schwab.h"
#include "containers/vector.h"
#include "data/stock.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/util/time_util.h"
#include "howling_tools/runfiles_init.h"
#include "logs/stdout_sink.h"

ABSL_FLAG(std::string, stock, "", "Stock symbol to fetch.");
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

using ::google::protobuf::Duration;
using ::google::protobuf::TextFormat;
using ::google::protobuf::Timestamp;
using ::google::protobuf::util::TimeUtil;
using ::std::chrono::system_clock;

char to_upper(char c) { return std::toupper(c); }

system_clock::time_point to_std_chrono(absl::Time time) {
  using namespace ::std::chrono;
  return absl::ToChronoTime(time);
}

auto to_std_chrono(absl::Duration duration) {
  return absl::ToChronoMicroseconds(duration);
}

Timestamp to_proto(system_clock::time_point time) {
  using namespace ::std::chrono;
  return TimeUtil::MicrosecondsToTimestamp(
      duration_cast<microseconds>(time.time_since_epoch()).count());
}

Duration to_proto(absl::Duration duration) {
  return TimeUtil::MicrosecondsToDuration(absl::ToInt64Microseconds(duration));
}

bool has_started_at() {
  return absl::GetFlag(FLAGS_start) != absl::UniversalEpoch();
}

system_clock::time_point get_started_at() {
  if (!has_started_at()) {
    return system_clock::now() - to_std_chrono(absl::GetFlag(FLAGS_duration));
  }
  return to_std_chrono(absl::GetFlag(FLAGS_start));
}

stock::Symbol get_stock_symbol() {
  std::string stock_name = absl::GetFlag(FLAGS_stock) |
      std::views::transform(to_upper) | std::ranges::to<std::string>();
  if (stock_name.empty())
    throw std::runtime_error("Must specify --stock flag.");
  stock::Symbol symbol;
  if (!stock::Symbol_Parse(stock_name, &symbol)) {
    throw std::runtime_error(
        absl::StrCat("Unknown stock symbol: ", stock_name, "."));
  }
  return symbol;
}

void run_alpaca() {
  stock::Symbol symbol = get_stock_symbol();
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
  stock::Symbol symbol = get_stock_symbol();
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

  vector<Candle> candles = schwab::get_history(
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
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  howling::StdoutLogSink log_sink;
  absl::AddLogSink(&log_sink);
  howling::initialize_runfiles(argv[0]);

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

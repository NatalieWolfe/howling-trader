#include "data/utilities.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "data/stock.pb.h"
#include "google/protobuf/text_format.h"
#include "strings/format.h"

namespace howling {
namespace {

namespace fs = ::std::filesystem;

using ::google::protobuf::TextFormat;

const fs::path HISTORY_DIR = "howling-trader/data/history";

} // namespace

fs::path get_history_file_path(stock::Symbol symbol, std::string_view date) {
  fs::path path = HISTORY_DIR / stock::Symbol_Name(symbol) / date;
  path += ".textproto";
  return path;
}

stock::History read_history(const fs::path& path) {
  if (!fs::exists(path)) {
    throw std::runtime_error(absl::StrCat("No data found at ", path.string()));
  }

  std::ifstream stream(path);
  std::stringstream data;
  data << stream.rdbuf();

  stock::History history;
  if (!TextFormat::ParseFromString(data.str(), &history)) {
    throw std::runtime_error(
        absl::StrCat("Failed to parse contents of ", path.string()));
  }
  return history;
}

stock::Symbol get_stock_symbol(std::string_view name) {
  std::string stock_name =
      name | std::views::transform(to_upper) | std::ranges::to<std::string>();
  if (stock_name.empty()) {
    throw std::runtime_error("Stock name not specified.");
  }
  stock::Symbol symbol;
  if (!stock::Symbol_Parse(stock_name, &symbol)) {
    throw std::runtime_error(
        absl::StrCat("Unknown stock symbol: ", stock_name, "."));
  }
  return symbol;
}

} // namespace howling

#include "data/utilities.h"

#include <filesystem>
#include <fstream>
#include <generator>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "data/stock.pb.h"
#include "google/protobuf/text_format.h"
#include "strings/format.h"

namespace howling::stock {

bool AbslParseFlag(absl::string_view text, Symbol* symbol, std::string* error) {
  std::string stock_name =
      text | std::views::transform(to_upper) | std::ranges::to<std::string>();
  if (Symbol_Parse(stock_name, symbol)) return true;
  *error = absl::StrCat("Unknown stock symbol: ", text);
  return false;
}

std::string AbslUnparseFlag(Symbol symbol) {
  return Symbol_Name(symbol);
}

bool AbslParseFlag(
    absl::string_view text, std::vector<Symbol>* symbols, std::string* error) {
  for (absl::string_view s : absl::StrSplit(text, ',', absl::SkipEmpty())) {
    Symbol symbol;
    if (!AbslParseFlag(s, &symbol, error)) return false;
    symbols->push_back(symbol);
  }
  return true;
}

std::string AbslUnparseFlag(const std::vector<Symbol>& symbols) {
  std::vector<std::string> parts;
  for (Symbol s : symbols) parts.push_back(Symbol_Name(s));
  return absl::StrJoin(parts, ",");
}

} // namespace howling::stock

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

std::generator<stock::Symbol> list_stock_symbols() {
  const auto& enum_descriptor = *stock::Symbol_descriptor();
  for (int i = 0; i < enum_descriptor.value_count(); ++i) {
    auto* value_descriptor = enum_descriptor.value(i);
    if (value_descriptor && value_descriptor->number() != 0) {
      co_yield static_cast<stock::Symbol>(enum_descriptor.value(i)->number());
    }
  }
}

} // namespace howling

#pragma once

#include <filesystem>
#include <generator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "data/stock.pb.h"

namespace howling::stock {

bool AbslParseFlag(absl::string_view text, Symbol* symbol, std::string* error);
std::string AbslUnparseFlag(Symbol symbol);

bool AbslParseFlag(
    absl::string_view text, std::vector<Symbol>* symbols, std::string* error);
std::string AbslUnparseFlag(const std::vector<Symbol>& symbols);

} // namespace howling::stock

namespace howling {

std::filesystem::path
get_history_file_path(stock::Symbol symbol, std::string_view date);
stock::History read_history(const std::filesystem::path& path);

std::generator<stock::Symbol> list_stock_symbols();

} // namespace howling

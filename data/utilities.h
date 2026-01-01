#pragma once

#include <filesystem>
#include <generator>
#include <string_view>

#include "data/stock.pb.h"

namespace howling {

std::filesystem::path
get_history_file_path(stock::Symbol symbol, std::string_view date);
stock::History read_history(const std::filesystem::path& path);

stock::Symbol get_stock_symbol(absl::string_view name);

std::generator<stock::Symbol> list_stock_symbols();

} // namespace howling

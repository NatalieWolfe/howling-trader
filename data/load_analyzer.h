#pragma once

#include <memory>
#include <string_view>

#include "data/analyzer.h"
#include "data/stock.pb.h"

namespace howling {

std::unique_ptr<analyzer>
load_analyzer(std::string_view name, const stock::History& history);

std::unique_ptr<analyzer> load_analyzer(std::string_view name);

} // namespace howling

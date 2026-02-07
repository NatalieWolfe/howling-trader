#pragma once

#include <string_view>

#include "json/json.h"

namespace howling {

Json::Value to_json(std::string_view str);

} // namespace howling

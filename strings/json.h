#pragma once

#include <string_view>

#include "json/value.h"

namespace howling {

Json::Value to_json(std::string_view str);
std::string to_string(const Json::Value& json);

} // namespace howling

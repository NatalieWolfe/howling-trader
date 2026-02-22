#pragma once

#include <string>

#include "absl/flags/declare.h"

ABSL_DECLARE_FLAG(std::string, schwab_api_key_id);
ABSL_DECLARE_FLAG(std::string, schwab_api_key_secret);
ABSL_DECLARE_FLAG(std::string, schwab_oauth_redirect_url);

namespace howling::schwab {

void check_schwab_flags();

} // namespace howling::schwab

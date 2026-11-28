#pragma once

#include <string>

#include "absl/flags/declare.h"

ABSL_DECLARE_FLAG(std::string, schwab_api_key_id);
ABSL_DECLARE_FLAG(std::string, schwab_api_key_secret);
ABSL_DECLARE_FLAG(std::string, schwab_oauth_redirect_url);

namespace howling::schwab {

/**
 * @brief Fetches Schwab API secrets from the security client, injects them
 * into the global configuration via absl::SetFlag, and validates that all
 * required Schwab flags are non-empty.
 *
 * @throws std::runtime_error on failure or if any required flags are empty.
 */
void fetch_schwab_secrets();

/**
 * @brief Checks that all required Schwab flags are set.
 *
 * @throws std::runtime_error if any required flags are empty.
 */
void check_schwab_flags();

} // namespace howling::schwab

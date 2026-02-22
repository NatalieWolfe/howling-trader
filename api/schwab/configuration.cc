#include "api/schwab/configuration.h"

#include <stdexcept>
#include <string>

#include "absl/flags/flag.h"

ABSL_FLAG(std::string, schwab_api_key_id, "", "API key ID for Schwab.");
ABSL_FLAG(std::string, schwab_api_key_secret, "", "API secret for Schwab.");
ABSL_FLAG(
    std::string,
    schwab_oauth_redirect_url,
    "https://howling-auth.wolfe.dev/callback",
    "Redirect URL for Schwab OAuth.");

namespace howling::schwab {

void check_schwab_flags() {
  if (absl::GetFlag(FLAGS_schwab_api_key_id).empty()) {
    throw std::runtime_error("--schwab_api_key_id flag is required.");
  }
  if (absl::GetFlag(FLAGS_schwab_api_key_secret).empty()) {
    throw std::runtime_error("--schwab_api_key_secret flag is required.");
  }
  if (absl::GetFlag(FLAGS_schwab_oauth_redirect_url).empty()) {
    throw std::runtime_error("--schwab_oauth_redirect_url flag is required.");
  }
}

} // namespace howling::schwab

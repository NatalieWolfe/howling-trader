#include "api/schwab/configuration.h"

#include <stdexcept>
#include <string>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "services/security.h"
#include "json/value.h"

ABSL_FLAG(std::string, schwab_api_key_id, "", "API key ID for Schwab.");
ABSL_FLAG(std::string, schwab_api_key_secret, "", "API secret for Schwab.");
ABSL_FLAG(
    std::string,
    schwab_oauth_redirect_url,
    "https://howling-oauth.wolfe.dev/schwab/oauth-callback",
    "Redirect URL for Schwab OAuth.");

namespace howling::schwab {

void fetch_schwab_secrets(security_client& security) {
  LOG(INFO) << "Fetching secrets from OpenBao...";
  Json::Value schwab_secret = security.get_secret("howling/prod/schwab");
  absl::SetFlag(
      &FLAGS_schwab_api_key_id, schwab_secret["api_key_id"].asString());
  absl::SetFlag(
      &FLAGS_schwab_api_key_secret, schwab_secret["api_key_secret"].asString());
}

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

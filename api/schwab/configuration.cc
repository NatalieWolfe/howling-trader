#include "api/schwab/configuration.h"

#include <string>

#include "absl/flags/flag.h"

ABSL_FLAG(std::string, schwab_api_key_id, "", "API key ID for Schwab.");
ABSL_FLAG(std::string, schwab_api_key_secret, "", "API secret for Schwab.");

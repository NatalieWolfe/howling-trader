#pragma once

#include <string>
#include <string_view>

#include "net/connect.h"

namespace howling::schwab {

struct oauth_tokens {
  std::string access_token;
  std::string refresh_token;
  int expires_in;
};

// Generates the Schwab Authorization URL to which the user should be directed
// for manual authentication.
std::string make_schwab_authorize_url(
    std::string_view client_id, std::string_view redirect_url);

// Exchanges an authorization code for access and refresh tokens.
oauth_tokens exchange_code_for_tokens(
    net::connection& conn, std::string_view code);

// Refreshes the access token using a refresh token.
oauth_tokens refresh_tokens(
    net::connection& conn, std::string_view refresh_token);

} // namespace howling::schwab

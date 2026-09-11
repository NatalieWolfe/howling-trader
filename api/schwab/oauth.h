#pragma once

#include <string>
#include <string_view>

#include "net/connect.h"
#include "services/oauth/errors.h"

namespace howling::schwab {

struct oauth_tokens {
  std::string access_token;
  std::string refresh_token;
  int expires_in;
};

// Generates the Schwab Authorization URL to which the user should be directed
// for manual authentication.
std::string make_schwab_authorize_url(
    std::string_view client_id,
    std::string_view redirect_url,
    std::string_view state);

/**
 * @brief Exchanges an authorization code for access and refresh tokens.
 *
 * @throws auth_rejected_error if the server rejects authentication (HTTP
 * 400/401).
 * @throws std::runtime_error on other communication or server errors.
 */
oauth_tokens
exchange_code_for_tokens(net::connection& conn, std::string_view code);

/**
 * @brief Refreshes the access token using a refresh token.
 *
 * @throws auth_rejected_error if the server rejects authentication (HTTP
 * 400/401).
 * @throws std::runtime_error on other communication or server errors.
 */
oauth_tokens
refresh_tokens(net::connection& conn, std::string_view refresh_token);

} // namespace howling::schwab

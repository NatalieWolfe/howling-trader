#pragma once

#include <memory>
#include <string_view>

#include "net/connect.h"

namespace howling::schwab {

/**
 * Returns a bearer token to use for the API.
 *
 * In order, this method gets the bearer token from:
 *  - In-memory cache of the bearer token,
 *  - Refreshing the token from a disk-cached refresh token, or
 *  - Executing a fresh OAuth login sequence.
 *
 * The latter options will cache the retrieved bearer token in memory upon
 * success so subsequent calls will be quicker.
 */
std::string_view get_bearer_token(
    const std::unique_ptr<net::connection>& conn, bool clear_cache = false);

} // namespace howling::schwab

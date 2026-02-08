#pragma once

#include <string>
#include <string_view>

namespace howling::schwab {

// Generates the Schwab Authorization URL to which the user should be directed
// for manual authentication.
std::string make_schwab_authorize_url(
    std::string_view client_id, std::string_view redirect_url);

} // namespace howling::schwab

#pragma once

#include <string>
#include <string_view>

#include "net/url.h"

namespace howling::schwab {

std::string get_schwab_host();

net::url make_net_url(std::string target);

} // namespace howling::schwab

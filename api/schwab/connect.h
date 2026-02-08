#pragma once

#include <string>

#include "net/url.h"

namespace howling::schwab {

std::string get_schwab_host();

net::url make_net_url(std::string target);

} // namespace howling::schwab

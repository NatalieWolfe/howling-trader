#pragma once

#include <string_view>

#include "api/schwab/oauth.h"
#include "net/connect.h"
#include "services/oauth/oauth_exchanger.h"

namespace howling {

class oauth_exchanger_impl : public oauth_exchanger {
public:
  schwab::oauth_tokens
  exchange(std::string_view code) override;
};

using real_oauth_exchanger [[deprecated("Use oauth_exchanger_impl instead")]] =
    oauth_exchanger_impl;

} // namespace howling

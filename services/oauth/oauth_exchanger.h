#pragma once

#include <string_view>

#include "api/schwab/oauth.h"
#include "net/connect.h"

namespace howling {

class oauth_exchanger {
public:
  virtual ~oauth_exchanger() = default;
  virtual schwab::oauth_tokens
  exchange(std::string_view code) = 0;
};

} // namespace howling

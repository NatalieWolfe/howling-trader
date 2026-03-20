#pragma once

#include <string_view>

#include "api/schwab/oauth.h"
#include "services/authenticate.h"
#include "gmock/gmock.h"

namespace howling {

class mock_token_refresher : public token_refresher {
public:
  MOCK_METHOD(
      schwab::oauth_tokens,
      refresh_tokens,
      (std::string_view refresh_token),
      (override));
};

} // namespace howling

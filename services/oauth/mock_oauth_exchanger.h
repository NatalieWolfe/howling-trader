#pragma once

#include <string_view>

#include "api/schwab/oauth.h"
#include "gmock/gmock.h"
#include "net/connect.h"
#include "services/oauth/oauth_exchanger.h"

namespace howling {

class mock_oauth_exchanger : public oauth_exchanger {
public:
  MOCK_METHOD(
      schwab::oauth_tokens,
      exchange,
      (std::string_view code),
      (override));
};

} // namespace howling

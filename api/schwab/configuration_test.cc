#include "api/schwab/configuration.h"

#include <memory>
#include <stdexcept>
#include <string>

#include "absl/flags/flag.h"
#include "services/mock_security.h"
#include "services/registry/register_service.h"
#include "services/registry/test_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "json/value.h"

namespace howling::schwab {
namespace {

using ::testing::Return;

class SchwabConfigurationTest : public testing::Test {
protected:
  void SetUp() override {
    _saved_api_key_id = absl::GetFlag(FLAGS_schwab_api_key_id);
    _saved_api_key_secret = absl::GetFlag(FLAGS_schwab_api_key_secret);
    _saved_oauth_redirect_url = absl::GetFlag(FLAGS_schwab_oauth_redirect_url);

    clear_service_registry_cache();
    auto mock_security = std::make_unique<mock_security_client>();
    _mock_security = mock_security.get();
    registry::register_service<security_client>(std::move(mock_security));
  }

  void TearDown() override {
    clear_service_registry_cache();
    absl::SetFlag(&FLAGS_schwab_api_key_id, _saved_api_key_id);
    absl::SetFlag(&FLAGS_schwab_api_key_secret, _saved_api_key_secret);
    absl::SetFlag(&FLAGS_schwab_oauth_redirect_url, _saved_oauth_redirect_url);
  }

  mock_security_client* _mock_security = nullptr;
  std::string _saved_api_key_id;
  std::string _saved_api_key_secret;
  std::string _saved_oauth_redirect_url;
};

TEST_F(SchwabConfigurationTest, FetchSchwabSecretsSetsFlagsAndValidates) {
  Json::Value secret;
  secret["api_key"] = "test-api-key";
  secret["api_secret"] = "test-api-secret";
  EXPECT_CALL(*_mock_security, get_secret("howling/prod/schwab"))
      .WillOnce(Return(secret));

  EXPECT_NO_THROW(fetch_schwab_secrets());
  EXPECT_EQ(absl::GetFlag(FLAGS_schwab_api_key_id), "test-api-key");
  EXPECT_EQ(absl::GetFlag(FLAGS_schwab_api_key_secret), "test-api-secret");
}

TEST_F(SchwabConfigurationTest, FetchSchwabSecretsThrowsOnEmptyApiKey) {
  Json::Value secret;
  secret["api_key"] = "";
  secret["api_secret"] = "test-api-secret";
  EXPECT_CALL(*_mock_security, get_secret("howling/prod/schwab"))
      .WillOnce(Return(secret));

  EXPECT_THROW(fetch_schwab_secrets(), std::runtime_error);
}

TEST_F(SchwabConfigurationTest, FetchSchwabSecretsThrowsOnEmptyApiSecret) {
  Json::Value secret;
  secret["api_key"] = "test-api-key";
  secret["api_secret"] = "";
  EXPECT_CALL(*_mock_security, get_secret("howling/prod/schwab"))
      .WillOnce(Return(secret));

  EXPECT_THROW(fetch_schwab_secrets(), std::runtime_error);
}

TEST_F(SchwabConfigurationTest, FetchSchwabSecretsThrowsOnMissingFields) {
  Json::Value secret;
  EXPECT_CALL(*_mock_security, get_secret("howling/prod/schwab"))
      .WillOnce(Return(secret));

  EXPECT_THROW(fetch_schwab_secrets(), std::runtime_error);
}

TEST_F(SchwabConfigurationTest, CheckSchwabFlagsThrowsOnEmptyRedirectUrl) {
  absl::SetFlag(&FLAGS_schwab_api_key_id, "test-api-key");
  absl::SetFlag(&FLAGS_schwab_api_key_secret, "test-api-secret");
  absl::SetFlag(&FLAGS_schwab_oauth_redirect_url, "");

  EXPECT_THROW(check_schwab_flags(), std::runtime_error);
}

} // namespace
} // namespace howling::schwab

#include "services/oauth/auth_client.h"

#include <memory>

#include "absl/flags/flag.h"
#include "grpcpp/grpcpp.h"
#include "services/mock_security.h"
#include "services/registry/register_service.h"
#include "services/registry/registry.h"
#include "services/security/register.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using ::testing::Return;

class AuthClientTest : public testing::Test {
protected:
  void SetUp() override {
    _mock_security = new mock_security_client();
    registry::register_service_factory([this]() {
      return std::unique_ptr<security_client>(this->_mock_security);
    });
  }

  mock_security_client* _mock_security;
};

TEST_F(AuthClientTest, MakeAuthServiceStubUsesSecureCredentials) {
  EXPECT_CALL(*_mock_security, get_ca_certificate())
      .WillOnce(Return("test_ca_cert"));

  // This should not crash and should return a valid stub.
  EXPECT_NE(make_auth_service_stub(), nullptr);
}

} // namespace
} // namespace howling

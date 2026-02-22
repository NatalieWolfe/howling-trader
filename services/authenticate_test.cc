#include "services/authenticate.h"

#include <chrono>
#include <future>
#include <memory>
#include <string>

#include "google/protobuf/empty.pb.h"
#include "grpcpp/grpcpp.h"
#include "services/mock_database.h"
#include "services/oauth/mock_auth_service.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"
#include "services/oauth/proto/auth_service.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using ::testing::_;
using ::testing::Return;

class MockTokenRefresher : public token_refresher {
public:
  MOCK_METHOD(
      schwab::oauth_tokens,
      refresh_tokens,
      (std::string_view refresh_token),
      (override));
};

class AuthenticateTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto stub = std::make_unique<MockAuthServiceStub>();
    auto db = std::make_unique<mock_database>();
    auto refresher = std::make_unique<MockTokenRefresher>();
    _stub = stub.get();
    _db = db.get();
    _refresher = refresher.get();

    _manager = std::make_unique<token_manager>(
        std::move(stub), std::move(db), std::move(refresher));
  }

  MockAuthServiceStub* _stub;
  mock_database* _db;
  MockTokenRefresher* _refresher;
  std::unique_ptr<token_manager> _manager;
};

TEST_F(AuthenticateTest, GetBearerTokenFromCache) {
  // First call to populate cache.
  std::promise<std::string> p;
  p.set_value("refresh_token_1");
  EXPECT_CALL(*_db, read_refresh_token("schwab"))
      .WillOnce(Return(p.get_future()));

  EXPECT_CALL(*_refresher, refresh_tokens("refresh_token_1"))
      .WillOnce(Return(
          schwab::oauth_tokens{
              .access_token = "access_token_1",
              .refresh_token = "refresh_token_1",
              .expires_in = 3600}));

  EXPECT_EQ(_manager->get_bearer_token(), "access_token_1");

  // Second call should return from cache without DB or refresher calls.
  EXPECT_EQ(_manager->get_bearer_token(), "access_token_1");
}

TEST_F(AuthenticateTest, GetBearerTokenRefreshesWhenExpired) {
  std::promise<std::string> p1;
  p1.set_value("refresh_token_1");
  EXPECT_CALL(*_db, read_refresh_token("schwab"))
      .WillOnce(Return(p1.get_future()));

  EXPECT_CALL(*_refresher, refresh_tokens("refresh_token_1"))
      .WillOnce(Return(
          schwab::oauth_tokens{
              .access_token = "access_token_1",
              .refresh_token = "refresh_token_1",
              .expires_in = 3600}));

  EXPECT_EQ(_manager->get_bearer_token(), "access_token_1");

  // Force clear cache
  EXPECT_CALL(*_db, read_refresh_token("schwab"))
      .WillOnce([&](std::string_view) {
        std::promise<std::string> p;
        p.set_value("refresh_token_2");
        return p.get_future();
      });

  EXPECT_CALL(*_refresher, refresh_tokens("refresh_token_2"))
      .WillOnce(Return(
          schwab::oauth_tokens{
              .access_token = "access_token_2",
              .refresh_token = "refresh_token_2",
              .expires_in = 3600}));

  EXPECT_EQ(_manager->get_bearer_token(true), "access_token_2");
}

TEST_F(AuthenticateTest, RequestLoginWhenNoRefreshToken) {
  EXPECT_CALL(*_db, read_refresh_token("schwab"))
      .WillOnce([](std::string_view) {
        std::promise<std::string> p;
        p.set_value("");
        return p.get_future();
      })
      .WillOnce([](std::string_view) {
        std::promise<std::string> p;
        p.set_value("new_refresh_token");
        return p.get_future();
      });

  EXPECT_CALL(*_stub, RequestLogin(_, _, _)).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*_refresher, refresh_tokens("new_refresh_token"))
      .WillOnce(Return(
          schwab::oauth_tokens{
              .access_token = "new_access_token",
              .refresh_token = "new_refresh_token",
              .expires_in = 3600}));

  EXPECT_EQ(_manager->get_bearer_token(), "new_access_token");
}

TEST_F(AuthenticateTest, UpdatesRefreshTokenIfChanged) {
  std::promise<std::string> p;
  p.set_value("old_refresh_token");
  EXPECT_CALL(*_db, read_refresh_token("schwab"))
      .WillOnce(Return(p.get_future()));

  EXPECT_CALL(*_refresher, refresh_tokens("old_refresh_token"))
      .WillOnce(Return(
          schwab::oauth_tokens{
              .access_token = "access_token",
              .refresh_token = "new_refresh_token",
              .expires_in = 3600}));

  std::promise<void> save_p;
  save_p.set_value();
  EXPECT_CALL(*_db, save_refresh_token("schwab", "new_refresh_token"))
      .WillOnce(Return(save_p.get_future()));

  EXPECT_EQ(_manager->get_bearer_token(), "access_token");
}

TEST_F(AuthenticateTest, TimeoutsWhenNoTokenAvailable) {
  EXPECT_CALL(*_db, read_refresh_token("schwab"))
      .WillRepeatedly([](std::string_view) {
        std::promise<std::string> p;
        p.set_value("");
        return p.get_future();
      });

  EXPECT_CALL(*_stub, RequestLogin(_, _, _))
      .WillRepeatedly(Return(grpc::Status::OK));

  EXPECT_THROW(
      _manager->get_bearer_token(false, std::chrono::milliseconds(500)),
      std::runtime_error);
}

} // namespace
} // namespace howling

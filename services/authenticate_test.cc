#include "services/authenticate.h"

#include <chrono>
#include <future>
#include <memory>
#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/time/time.h"
#include "google/protobuf/empty.pb.h"
#include "grpcpp/grpcpp.h"
#include "services/mock_database.h"
#include "services/mock_token_refresher.h"
#include "services/oauth/mock_auth_service.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"
#include "services/oauth/proto/auth_service.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

ABSL_DECLARE_FLAG(absl::Duration, auth_token_cache_ttl);
ABSL_DECLARE_FLAG(absl::Duration, auth_token_pump_period);

namespace howling {
namespace {

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

class AuthenticateTest : public ::testing::Test {
protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_auth_token_cache_ttl, absl::Seconds(1));
    absl::SetFlag(&FLAGS_auth_token_pump_period, absl::Milliseconds(100));

    auto stub = std::make_unique<mock_auth_service_stub>();
    auto refresher = std::make_unique<mock_token_refresher>();
    _stub = stub.get();
    _refresher = refresher.get();

    ON_CALL(_db, read_refresh_token("schwab"))
        .WillByDefault(InvokeWithoutArgs([]() {
          std::promise<std::string> p;
          p.set_value("");
          return p.get_future();
        }));

    ON_CALL(*_stub, RequestLogin(_, _, _))
        .WillByDefault(Return(grpc::Status::OK));

    _manager = std::make_unique<token_manager>(
        defer_pump_start, std::move(stub), _db, std::move(refresher));
  }

  mock_auth_service_stub* _stub;
  mock_database _db;
  mock_token_refresher* _refresher;
  std::unique_ptr<token_manager> _manager;
};

TEST_F(AuthenticateTest, GetBearerTokenFromCache) {
  // First call to populate cache.
  std::promise<std::string> p;
  p.set_value("refresh_token_1");
  EXPECT_CALL(_db, read_refresh_token("schwab"))
      .WillOnce(Return(p.get_future()));

  EXPECT_CALL(*_refresher, refresh_tokens("refresh_token_1"))
      .WillOnce(Return(
          schwab::oauth_tokens{
              .access_token = "access_token_1",
              .refresh_token = "refresh_token_1",
              .expires_in = 3600}));

  _manager->start_pump(); // Only necessary when pump start is deferred.
  EXPECT_EQ(_manager->get_bearer_token(), "access_token_1");

  // Second call should return from cache without DB or refresher calls.
  EXPECT_EQ(_manager->get_bearer_token(), "access_token_1");
}

TEST_F(AuthenticateTest, GetBearerTokenRefreshesWhenExpired) {
  std::promise<std::string> p1;
  p1.set_value("refresh_token_1");
  EXPECT_CALL(_db, read_refresh_token("schwab"))
      .WillOnce(Return(p1.get_future()));

  EXPECT_CALL(*_refresher, refresh_tokens("refresh_token_1"))
      .WillOnce(Return(
          schwab::oauth_tokens{
              .access_token = "access_token_1",
              .refresh_token = "refresh_token_1",
              .expires_in = 3600}));

  _manager->start_pump(); // Only necessary when pump start is deferred.
  EXPECT_EQ(_manager->get_bearer_token(), "access_token_1");

  // Force clear cache
  EXPECT_CALL(_db, read_refresh_token("schwab"))
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
  EXPECT_CALL(_db, read_refresh_token("schwab"))
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

  _manager->start_pump(); // Only necessary when pump start is deferred.
  EXPECT_EQ(_manager->get_bearer_token(), "new_access_token");
}

TEST_F(AuthenticateTest, UpdatesRefreshTokenIfChanged) {
  std::promise<std::string> p;
  p.set_value("old_refresh_token");
  EXPECT_CALL(_db, read_refresh_token("schwab"))
      .WillOnce(Return(p.get_future()));

  EXPECT_CALL(*_refresher, refresh_tokens("old_refresh_token"))
      .WillOnce(Return(
          schwab::oauth_tokens{
              .access_token = "access_token",
              .refresh_token = "new_refresh_token",
              .expires_in = 3600}));

  std::promise<void> save_p;
  save_p.set_value();
  EXPECT_CALL(_db, save_refresh_token("schwab", "new_refresh_token"))
      .WillOnce(Return(save_p.get_future()));

  _manager->start_pump(); // Only necessary when pump start is deferred.
  EXPECT_EQ(_manager->get_bearer_token(), "access_token");
}

TEST_F(AuthenticateTest, TimeoutsWhenNoTokenAvailable) {
  EXPECT_CALL(_db, read_refresh_token("schwab"))
      .WillRepeatedly([](std::string_view) {
        std::promise<std::string> p;
        p.set_value("");
        return p.get_future();
      });

  EXPECT_CALL(*_stub, RequestLogin(_, _, _))
      .WillRepeatedly(Return(grpc::Status::OK));

  _manager->start_pump(); // Only necessary when pump start is deferred.
  EXPECT_THROW(
      _manager->get_bearer_token(false, std::chrono::milliseconds(500)),
      std::runtime_error);
}

} // namespace
} // namespace howling

#include "services/oauth/auth_service.h"

#include <chrono>
#include <future>
#include <memory>
#include <optional>

#include "absl/flags/flag.h"
#include "api/schwab/configuration.h"
#include "google/protobuf/empty.pb.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/support/channel_arguments.h"
#include "grpcpp/support/status.h"
#include "services/mock_database.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"
#include "services/oauth/proto/auth_service.pb.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using ::std::chrono::system_clock;
using ::testing::Return;

class AuthServiceTest : public ::testing::Test {
protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_schwab_api_key_id, "test_id");
    absl::SetFlag(&FLAGS_schwab_api_key_secret, "test_secret");
    _service = std::make_unique<auth_service>(_db);
    grpc::ServerBuilder builder;
    builder.RegisterService(_service.get());
    _server = builder.BuildAndStart();
    _channel = _server->InProcessChannel(grpc::ChannelArguments());
    _stub = AuthService::NewStub(_channel);
  }

  void TearDown() override { _server->Shutdown(); }

  mock_database _db;
  std::unique_ptr<auth_service> _service;
  std::unique_ptr<grpc::Server> _server;
  std::shared_ptr<grpc::Channel> _channel;
  std::unique_ptr<AuthService::Stub> _stub;
};

TEST_F(AuthServiceTest, RequestLoginSkipsIfNotifiedRecently) {
  LoginRequest request;
  request.set_service_name("schwab");
  google::protobuf::Empty response;
  grpc::ClientContext context;

  std::promise<std::optional<system_clock::time_point>> p;
  p.set_value(system_clock::now());
  EXPECT_CALL(_db, get_last_notified_at("schwab"))
      .WillOnce(Return(p.get_future()));

  grpc::Status status = _stub->RequestLogin(&context, request, &response);

  EXPECT_TRUE(status.ok());
}

TEST_F(AuthServiceTest, RequestLoginReturnsUnimplementedOnFirstAttempt) {
  LoginRequest request;
  request.set_service_name("schwab");
  google::protobuf::Empty response;
  grpc::ClientContext context;

  std::promise<std::optional<system_clock::time_point>> p;
  p.set_value(std::nullopt);
  EXPECT_CALL(_db, get_last_notified_at("schwab"))
      .WillOnce(Return(p.get_future()));

  std::promise<void> p_sent;
  p_sent.set_value();
  EXPECT_CALL(_db, update_last_notified_at("schwab"))
      .WillOnce(Return(p_sent.get_future()));

  grpc::Status status = _stub->RequestLogin(&context, request, &response);

  EXPECT_EQ(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

} // namespace
} // namespace howling

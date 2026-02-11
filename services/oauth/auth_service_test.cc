#include "services/oauth/auth_service.h"

#include <memory>

#include <grpcpp/support/channel_arguments.h>
#include <grpcpp/support/status.h>

#include "absl/flags/flag.h"
#include "api/schwab/configuration.h"
#include "google/protobuf/empty.pb.h"
#include "grpcpp/grpcpp.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"
#include "services/oauth/proto/auth_service.pb.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

class auth_service_test : public ::testing::Test {
protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_schwab_api_key_id, "test_id");
    absl::SetFlag(&FLAGS_schwab_api_key_secret, "test_secret");
    _service = std::make_unique<auth_service>();
    grpc::ServerBuilder builder;
    builder.RegisterService(_service.get());
    _server = builder.BuildAndStart();
    _channel = _server->InProcessChannel(grpc::ChannelArguments());
    _stub = AuthService::NewStub(_channel);
  }

  void TearDown() override { _server->Shutdown(); }

  std::unique_ptr<auth_service> _service;
  std::unique_ptr<grpc::Server> _server;
  std::shared_ptr<grpc::Channel> _channel;
  std::unique_ptr<AuthService::Stub> _stub;
};

TEST_F(auth_service_test, request_login_returns_unimplemented) {
  LoginRequest request;
  request.set_service_name("schwab");
  google::protobuf::Empty response;
  grpc::ClientContext context;

  grpc::Status status = _stub->RequestLogin(&context, request, &response);

  EXPECT_EQ(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

} // namespace
} // namespace howling

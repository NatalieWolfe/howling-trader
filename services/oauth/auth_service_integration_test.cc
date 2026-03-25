#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include "absl/flags/flag.h"
#include "environment/runfiles.h"
#include "files/files.h"
#include "google/protobuf/empty.pb.h"
#include "grpcpp/grpcpp.h"
#include "services/db/schema/auth_token.h"
#include "services/mock_database.h"
#include "services/oauth/auth_service.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"
#include "services/oauth/proto/auth_service.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using ::testing::Return;

auto make_server_credentials(std::string_view cert) {
  grpc::SslServerCredentialsOptions ssl_opts;
  ssl_opts.pem_root_certs = cert;
  ssl_opts.pem_key_cert_pairs.push_back(
      {.private_key =
           files::read_file(runfile("howling-trader/net/local.wolfe.dev.key")),
       .cert_chain = std::string{cert}});
  return grpc::SslServerCredentials(ssl_opts);
}

class AuthServiceIntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    _service = std::make_unique<auth_service>(_db);
    std::string cert =
        files::read_file(runfile("howling-trader/net/local.wolfe.dev.crt"));

    grpc::ServerBuilder builder;
    builder.AddListeningPort(
        "127.0.0.1:0", make_server_credentials(cert), &_port);
    builder.RegisterService(_service.get());
    _server = builder.BuildAndStart();

    grpc::SslCredentialsOptions ssl_call_opts;
    ssl_call_opts.pem_root_certs = cert;
    grpc::ChannelArguments args;
    args.SetSslTargetNameOverride("local.wolfe.dev");
    _channel = grpc::CreateCustomChannel(
        "127.0.0.1:" + std::to_string(_port),
        grpc::SslCredentials(ssl_call_opts),
        args);
    _stub = AuthService::NewStub(_channel);
  }

  void TearDown() override { _server->Shutdown(); }

  int _port = 0;
  mock_database _db;
  std::unique_ptr<auth_service> _service;
  std::unique_ptr<grpc::Server> _server;
  std::shared_ptr<grpc::Channel> _channel;
  std::unique_ptr<AuthService::Stub> _stub;
};

TEST_F(AuthServiceIntegrationTest, SecureConnectionWorks) {
  LoginRequest request;
  request.set_service_name("schwab");
  google::protobuf::Empty response;
  grpc::ClientContext context;

  std::promise<std::optional<storage::auth_token>> p;
  p.set_value(
      storage::auth_token{
          .last_notified_at = std::chrono::system_clock::now()});
  EXPECT_CALL(_db, get_auth_token("schwab")).WillOnce(Return(p.get_future()));

  grpc::Status status = _stub->RequestLogin(&context, request, &response);
  EXPECT_TRUE(status.ok()) << "gRPC failed with error: "
                           << status.error_message();
}

} // namespace
} // namespace howling

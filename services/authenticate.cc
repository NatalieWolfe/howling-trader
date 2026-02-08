#include "services/authenticate.h"

#include "absl/log/log.h"
#include "google/protobuf/empty.pb.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/support/status.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"
#include "services/oauth/proto/auth_service.pb.h"

namespace howling {

struct token_manager::implementation {
  std::unique_ptr<AuthService::Stub> stub;

  implementation() {
    // TODO: Take in the auth service hostname/ip and port on the command line.
    // TODO: Use secure channel credentials for communication.
    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
        "localhost:50051", grpc::InsecureChannelCredentials());
    stub = AuthService::NewStub(channel);
  }
};

token_manager::token_manager()
    : _implementation(std::make_unique<implementation>()) {}

token_manager::~token_manager() = default;

std::string token_manager::get_bearer_token(std::chrono::milliseconds timeout) {
  // Logic:
  // 1. Check DB for valid Refresh Token.
  // 2. If found:
  //    a. Call Schwab API to exchange Refresh Token for a new memory-only
  //       Access Token.
  //    b. Return Access Token.
  // 3. If missing/invalid:
  //    a. Call Auth Service gRPC: RequestLogin().
  //    b. Poll DB until a new Refresh Token appears.
  //    c. Exchange new Refresh Token for Access Token.
  //    d. Return Access Token.

  // If missing/invalid, request manual auth via gRPC.
  LoginRequest request;
  request.set_service_name("schwab");
  google::protobuf::Empty response;
  grpc::ClientContext context;

  auto deadline = std::chrono::system_clock::now() + timeout;
  context.set_deadline(deadline);

  grpc::Status status =
      _implementation->stub->RequestLogin(&context, request, &response);

  if (!status.ok()) {
    LOG(ERROR) << "gRPC RequestLogin failed: " << status.error_message();
    return "";
  }

  // TODO: Poll DB until new tokens appear.
  LOG(INFO)
      << "Manual authentication requested successfully. Waiting for tokens...";

  // Placeholder: just return empty for now since polling isn't implemented.
  return "";
}

} // namespace howling

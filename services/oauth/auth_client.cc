#include "services/oauth/auth_client.h"

#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/credentials.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"

ABSL_FLAG(
    std::string,
    auth_service_address,
    "localhost:50051",
    "Address of the howling.AuthService.");

namespace howling {

std::unique_ptr<AuthService::Stub>
make_auth_service_stub(std::shared_ptr<grpc::Channel> channel) {
  if (!channel) {
    channel = grpc::CreateChannel(
        absl::GetFlag(FLAGS_auth_service_address),
        grpc::InsecureChannelCredentials());
  }
  return AuthService::NewStub(channel);
}

} // namespace howling

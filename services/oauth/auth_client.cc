#include "services/oauth/auth_client.h"

#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/credentials.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"
#include "services/registry/registry.h"
#include "services/security.h"

ABSL_FLAG(
    std::string,
    auth_service_address,
    "localhost:50051",
    "Address of the howling.AuthService.");

ABSL_FLAG(
    std::string,
    auth_service_ssl_target_name,
    "howling-oauth.howling-app.svc.cluster.local",
    "Expected TLS server name for the howling.AuthService.");

namespace howling {
namespace {

auto make_channel_credentials() {
  grpc::SslCredentialsOptions ssl_options;
  ssl_options.pem_root_certs =
      registry::get_service<security_client>().get_ca_certificate();
  return grpc::SslCredentials(ssl_options);
}

} // namespace

std::unique_ptr<AuthService::Stub>
make_auth_service_stub(std::shared_ptr<grpc::Channel> channel) {
  // TODO: Refactor this into a service registry factory.
  if (!channel) {
    grpc::ChannelArguments args;
    std::string ssl_target = absl::GetFlag(FLAGS_auth_service_ssl_target_name);
    if (!ssl_target.empty()) { args.SetSslTargetNameOverride(ssl_target); }
    channel = grpc::CreateCustomChannel(
        absl::GetFlag(FLAGS_auth_service_address),
        make_channel_credentials(),
        args);
  }
  return AuthService::NewStub(channel);
}

} // namespace howling

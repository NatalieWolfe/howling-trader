#include "services/oauth/auth_service.h"

#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

#include <string>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "api/schwab/configuration.h"
#include "api/schwab/oauth.h"

namespace howling {
namespace {

constexpr std::string_view REDIRECT_URL =
    "https://howling-auth.wolfe.dev/callback";

} // namespace

grpc::Status auth_service::RequestLogin(
    grpc::ServerContext* context,
    const LoginRequest* request,
    google::protobuf::Empty* response) {
  LOG(INFO) << "Received RequestLogin for service: " << request->service_name();

  if (request->service_name() == "schwab") {
    try {
      schwab::check_schwab_flags();
    } catch (const std::exception& e) {
      return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what());
    }
    std::string url = schwab::make_schwab_authorize_url(
        absl::GetFlag(FLAGS_schwab_api_key_id), REDIRECT_URL);
    LOG(INFO) << "Generated Schwab OAuth URL: " << url;
  } else {
    return grpc::Status(
        grpc::StatusCode::INVALID_ARGUMENT, "Unsupported service name");
  }

  // TODO: Send notification to user with the link.

  return grpc::Status(
      grpc::StatusCode::UNIMPLEMENTED, "Notification not implemented yet");
}

} // namespace howling

#include "services/oauth/auth_service.h"

#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

#include "absl/log/log.h"

namespace howling {

grpc::Status auth_service::RequestLogin(
    grpc::ServerContext* context,
    const LoginRequest* request,
    google::protobuf::Empty* response) {
  LOG(INFO) << "Received RequestLogin for service: " << request->service_name();

  // TODO: Generate Schwab OAuth URL.
  // TODO: Send notification to user with the link.

  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented yet");
}

} // namespace howling

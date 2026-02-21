#include "services/oauth/auth_service.h"

#include <chrono>
#include <string>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "api/schwab/configuration.h"
#include "api/schwab/oauth.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace howling {
namespace {

using ::std::chrono::system_clock;

constexpr std::chrono::minutes NOTIFICATION_COOLING_PERIOD{15};

bool should_notify_again(system_clock::time_point last_notified_at) {
  return system_clock::now() - last_notified_at > NOTIFICATION_COOLING_PERIOD;
}

} // namespace

grpc::Status auth_service::RequestLogin(
    grpc::ServerContext* context,
    const LoginRequest* request,
    google::protobuf::Empty* response) {
  LOG(INFO) << "Received RequestLogin for service: " << request->service_name();

  // Check if we should send a notification based on cooling period.
  auto last_notified_at =
      _db.get_last_notified_at(request->service_name()).get();
  if (last_notified_at && !should_notify_again(*last_notified_at)) {
    LOG(INFO) << "Skipping redundant notification for service: "
              << request->service_name();
    return grpc::Status::OK;
  }

  if (request->service_name() == "schwab") {
    try {
      schwab::check_schwab_flags();
    } catch (const std::exception& e) {
      return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what());
    }
    std::string url = schwab::make_schwab_authorize_url(
        absl::GetFlag(FLAGS_schwab_api_key_id),
        absl::GetFlag(FLAGS_schwab_oauth_redirect_url));
    LOG(INFO) << "Generated Schwab OAuth URL: " << url;
  } else {
    return grpc::Status(
        grpc::StatusCode::INVALID_ARGUMENT, "Unsupported service name");
  }

  // TODO: Send notification to user with the link.

  // After successfully sending notification:
  _db.update_last_notified_at(request->service_name()).get();
  return grpc::Status(
      grpc::StatusCode::UNIMPLEMENTED, "Notification not implemented yet");
}

} // namespace howling

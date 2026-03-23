#include "services/oauth/auth_service.h"

#include <chrono>
#include <format>
#include <random>
#include <string>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/strings/escaping.h"
#include "api/schwab/configuration.h"
#include "api/schwab/oauth.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"
#include "services/db/schema/auth_token.h"
#include "services/oauth/notification.h"

namespace howling {
namespace {

using ::std::chrono::system_clock;

constexpr std::chrono::minutes NOTIFICATION_COOLING_PERIOD{15};

std::string generate_notice_token() {
  std::random_device rand;
  union {
    uint64_t bits;
    char chars[8];
  } data;
  data.bits = rand();
  data.bits <<= 32;
  data.bits |= rand();
  return absl::WebSafeBase64Escape(std::string_view{data.chars, 8})
      .substr(0, 8);
}

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
  auto auth_token = _db.get_auth_token(request->service_name()).get();
  if (auth_token && auth_token->last_notified_at &&
      !should_notify_again(*auth_token->last_notified_at)) {
    LOG(INFO) << "Skipping redundant notification for service: "
              << request->service_name();
    return grpc::Status::OK;
  }

  std::string login_url;
  std::string notice_token = generate_notice_token();
  if (request->service_name() == "schwab") {
    try {
      schwab::check_schwab_flags();
    } catch (const std::exception& e) {
      return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what());
    }
    login_url = schwab::make_schwab_authorize_url(
        absl::GetFlag(FLAGS_schwab_api_key_id),
        absl::GetFlag(FLAGS_schwab_oauth_redirect_url),
        /*state=*/notice_token);
  } else {
    return grpc::Status(
        grpc::StatusCode::INVALID_ARGUMENT, "Unsupported service name");
  }

  try {
    std::string message = std::format(
        "Authentication required for {}: {}",
        request->service_name(),
        login_url);

    oauth::send_notification(message);
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to dispatch notification: " << e.what();
    return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
  }

  // After successfully sending notification:
  _db.save_notice_token(request->service_name(), notice_token).get();
  return grpc::Status::OK;
}

} // namespace howling

#include "services/authenticate.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>

#include "absl/log/log.h"
#include "api/schwab/connect.h"
#include "api/schwab/oauth.h"
#include "google/protobuf/empty.pb.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/support/status.h"
#include "net/connect.h"
#include "services/database.h"
#include "services/db/make_database.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"
#include "services/oauth/proto/auth_service.pb.h"

namespace howling {
namespace {

using namespace ::std::chrono_literals;

using ::std::chrono::system_clock;

// Schwab tokens usually last 30 minutes. Cache for 25 to be safe.
constexpr std::string_view SERVICE_NAME = "schwab";
constexpr auto CACHE_DURATION = 25min;

class real_token_refresher : public token_refresher {
public:
  schwab::oauth_tokens refresh_tokens(std::string_view refresh_token) override {
    std::unique_ptr<net::connection> conn =
        net::make_connection(schwab::make_net_url("/"));
    return schwab::refresh_tokens(*conn, refresh_token);
  }
};

} // namespace

struct token_manager::implementation {
  implementation() {
    // TODO: Take in the auth service hostname/ip and port on the command line.
    // TODO: Use secure channel credentials for communication.
    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
        "localhost:50051", grpc::InsecureChannelCredentials());
    stub = AuthService::NewStub(channel);
    db = make_database();
    refresher = std::make_unique<real_token_refresher>();
  }

  implementation(
      std::unique_ptr<AuthService::StubInterface> stub,
      std::unique_ptr<database> db,
      std::unique_ptr<token_refresher> refresher)
      : stub(std::move(stub)), db(std::move(db)),
        refresher(std::move(refresher)) {}

  std::string check_cache(bool clear_cache);

  std::unique_ptr<AuthService::StubInterface> stub;
  std::unique_ptr<database> db;
  std::unique_ptr<token_refresher> refresher;
  std::mutex mutex;
  std::string cached_token;
  system_clock::time_point cache_expiration;
};

std::string token_manager::implementation::check_cache(bool clear_cache) {
  std::lock_guard<std::mutex> lock(mutex);
  if (clear_cache || system_clock::now() > cache_expiration) {
    cached_token.clear();
  }
  return cached_token;
}

token_manager& token_manager::get_instance() {
  static token_manager instance;
  return instance;
}

token_manager::token_manager()
    : _implementation(std::make_unique<implementation>()) {}

token_manager::token_manager(
    std::unique_ptr<AuthService::StubInterface> stub,
    std::unique_ptr<database> db,
    std::unique_ptr<token_refresher> refresher)
    : _implementation(
          std::make_unique<implementation>(
              std::move(stub), std::move(db), std::move(refresher))) {}

token_manager::~token_manager() = default;

std::string token_manager::get_bearer_token(
    bool clear_cache, std::chrono::milliseconds timeout) {
  std::string refresh_token = _implementation->check_cache(clear_cache);
  if (!refresh_token.empty()) return refresh_token;

  system_clock::time_point deadline = system_clock::now() + timeout;
  while (system_clock::now() < deadline) {
    refresh_token = _implementation->db->read_refresh_token(SERVICE_NAME).get();

    if (!refresh_token.empty()) {
      try {
        schwab::oauth_tokens tokens =
            _implementation->refresher->refresh_tokens(refresh_token);

        if (tokens.refresh_token != refresh_token) {
          _implementation->db
              ->save_refresh_token(SERVICE_NAME, tokens.refresh_token)
              .get();
        }

        {
          std::lock_guard<std::mutex> lock(_implementation->mutex);
          _implementation->cached_token = tokens.access_token;
          _implementation->cache_expiration =
              system_clock::now() + CACHE_DURATION;
          return _implementation->cached_token;
        }
      } catch (const std::exception& e) {
        LOG(WARNING) << "Failed to refresh token: " << e.what();
      }
    }

    // If missing/invalid, request manual auth via gRPC.
    LoginRequest request;
    request.set_service_name(SERVICE_NAME);
    google::protobuf::Empty response;
    grpc::ClientContext context;
    context.set_deadline(system_clock::now() + 5s);
    grpc::Status status =
        _implementation->stub->RequestLogin(&context, request, &response);

    if (!status.ok()) {
      LOG(ERROR) << "gRPC RequestLogin failed: " << status.error_message();
    }

    std::this_thread::sleep_for(1s);
  }

  throw std::runtime_error("Timed out waiting for bearer token.");
}

} // namespace howling

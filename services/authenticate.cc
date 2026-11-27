#include "services/authenticate.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/time/time.h"
#include "api/schwab/connect.h"
#include "api/schwab/oauth.h"
#include "google/protobuf/empty.pb.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/credentials.h"
#include "grpcpp/support/status.h"
#include "net/connect.h"
#include "services/database.h"
#include "services/db/schema/auth_token.h"
#include "services/oauth/auth_client.h"
#include "services/oauth/errors.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"
#include "services/oauth/proto/auth_service.pb.h"
#include "services/registry/registry.h"
#include "signaling/cancellation.h"
#include "signaling/signal_handler.h"
#include "time/conversion.h"

ABSL_FLAG(
    absl::Duration,
    auth_token_cache_ttl,
    absl::Minutes(20),
    "Lifetime for cached auth tokens before they are refreshed.");
ABSL_FLAG(
    absl::Duration,
    auth_token_pump_period,
    absl::Seconds(20),
    "Waiting period between iterations of the token refresh pump.");

namespace howling {
namespace {

using ::std::chrono::system_clock;

// Schwab tokens usually last 30 minutes. Cache for 25 to be safe.
constexpr std::string_view SERVICE_NAME = "schwab";
constexpr double BACKOFF_RATE = 1.1;
constexpr std::chrono::milliseconds CANCEL_POLL_INTERVAL{100};

static token_manager* alternate_singleton = nullptr;

bool is_cancelled(
    std::optional<std::reference_wrapper<const cancellation>> cancel) noexcept {
  if (cancel) return cancel->get().is_cancelled();
  return signal_handler::is_initialized() &&
      signal_handler::instance().has_received_signal();
}

auto auth_token_cache_ttl() {
  return to_std_chrono(absl::GetFlag(FLAGS_auth_token_cache_ttl));
}

auto auth_token_pump_period() {
  return to_std_chrono(absl::GetFlag(FLAGS_auth_token_pump_period));
}

class real_token_refresher : public token_refresher {
public:
  schwab::oauth_tokens refresh_tokens(std::string_view refresh_token) override {
    std::unique_ptr<net::connection> conn =
        net::make_connection(schwab::make_net_url("/"));
    return schwab::refresh_tokens(*conn, refresh_token);
  }
};

} // namespace

// MARK: Token Manager Implementation

class token_manager::implementation {
public:
  implementation() : _db{registry::get_service<database>()} {
    _auth_stub = make_auth_service_stub();
    _refresher = std::make_unique<real_token_refresher>();
    _start_pump();
  }

  implementation(
      std::unique_ptr<AuthService::StubInterface> auth_stub,
      database& db,
      std::unique_ptr<token_refresher> refresher)
      : _auth_stub{std::move(auth_stub)}, _db{db},
        _refresher{std::move(refresher)} {
    if (!_refresher) _refresher = std::make_unique<real_token_refresher>();
    _start_pump();
  }

  implementation(
      defer_pump_start_t,
      std::unique_ptr<AuthService::StubInterface> auth_stub,
      database& db,
      std::unique_ptr<token_refresher> refresher)
      : _auth_stub{std::move(auth_stub)}, _db{db},
        _refresher{std::move(refresher)} {
    if (!_refresher) _refresher = std::make_unique<real_token_refresher>();
  }

  ~implementation() {
    {
      std::lock_guard lock{_pump_mutex};
      _continue_pumping = false;
    }
    _pump_cv.notify_all();
  }

  std::string check_cache(bool clear_cache);

  template <typename Rep, typename Period>
  std::string wait_for(
      std::chrono::duration<Rep, Period> timeout,
      std::optional<std::reference_wrapper<const cancellation>> cancel =
          std::nullopt) {
    if (is_cancelled(cancel)) return {};

    cancellation::callback_registration cancel_reg;
    if (cancel) {
      cancel_reg = cancel->get().on_cancel([this]() {
        std::lock_guard lock{_mutex};
        _token_refreshed.notify_all();
      });
    }

    const auto now = std::chrono::steady_clock::now();
    const auto steady_timeout =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            timeout);
    const auto max_remaining =
        std::chrono::steady_clock::time_point::max() - now;
    const auto deadline = steady_timeout >= max_remaining
        ? std::chrono::steady_clock::time_point::max()
        : now + steady_timeout;

    std::unique_lock lock{_mutex};
    while (_cached_token.empty() && !is_cancelled(cancel)) {
      const auto current = std::chrono::steady_clock::now();
      if (current >= deadline) break;
      const auto remaining = deadline - current;
      const auto slice = std::min(
          remaining,
          std::chrono::duration_cast<std::chrono::steady_clock::duration>(
              CANCEL_POLL_INTERVAL));
      _token_refreshed.wait_for(lock, slice, [&]() {
        return !_cached_token.empty() || is_cancelled(cancel);
      });
    }
    if (is_cancelled(cancel)) return {};
    return _cached_token;
  }

  void maybe_start_pump() {
    if (!_pump_thread.joinable()) _start_pump();
  }

private:
  void _start_pump();
  void _pump();
  void _request_login_notification();

  int _pump_failure_count = 0;
  std::unique_ptr<AuthService::StubInterface> _auth_stub;
  database& _db;
  std::unique_ptr<token_refresher> _refresher;
  std::mutex _pump_mutex;
  std::condition_variable _pump_cv;
  bool _continue_pumping = true;
  std::mutex _mutex;
  std::string _cached_token;
  std::string _cached_refresh_token;
  system_clock::time_point _cache_expiration;
  std::condition_variable _token_refreshed;
  std::jthread _pump_thread;
};

std::string token_manager::implementation::check_cache(bool clear_cache) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (clear_cache) {
    _cached_token.clear();
    _cached_refresh_token.clear();
  } else if (system_clock::now() > _cache_expiration) {
    _cached_token.clear();
  }
  return _cached_token;
}

void token_manager::implementation::_start_pump() {
  _pump_thread = std::jthread{[this]() {
    while (true) {
      {
        std::lock_guard lock{_pump_mutex};
        if (!_continue_pumping) break;
      }
      if (system_clock::now() > _cache_expiration) _pump();
      const auto sleep_duration = auth_token_pump_period() *
          std::pow(BACKOFF_RATE, _pump_failure_count);
      std::unique_lock lock{_pump_mutex};
      _pump_cv.wait_for(
          lock,
          std::chrono::duration_cast<std::chrono::steady_clock::duration>(
              sleep_duration),
          [this]() { return !_continue_pumping; });
      if (!_continue_pumping) break;
    }
  }};
}

void token_manager::implementation::_pump() {
  std::string refresh_token;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_cached_refresh_token.empty()) {
      try {
        auto auth_token = _db.get_auth_token(SERVICE_NAME).get();
        if (auth_token) _cached_refresh_token = auth_token->refresh_token;
      } catch (const std::exception& e) {
        LOG(WARNING) << "Error retrieving auth token from DB: " << e.what();
        _cached_refresh_token.clear();
      }
    }
    refresh_token = _cached_refresh_token;
  }

  if (refresh_token.empty() || _pump_failure_count > 10) {
    _pump_failure_count = 0;
    check_cache(/*clear_cache=*/true);
    _request_login_notification();
  } else {
    try {
      schwab::oauth_tokens tokens = _refresher->refresh_tokens(refresh_token);
      if (tokens.refresh_token != refresh_token) {
        _db.save_refresh_token(SERVICE_NAME, tokens.refresh_token).get();
      }

      {
        std::lock_guard<std::mutex> lock(_mutex);
        _cached_token = tokens.access_token;
        _cache_expiration = system_clock::now() + auth_token_cache_ttl();
        _token_refreshed.notify_all();
        _pump_failure_count = 0;
        return;
      }
    } catch (const auth_rejected_error& e) {
      LOG(WARNING) << "Authentication rejected by Schwab (permanent failure): "
                   << e.what();
      _pump_failure_count = 0;
      check_cache(/*clear_cache=*/true);
      _request_login_notification();
    } catch (const std::exception& e) {
      ++_pump_failure_count;
      LOG(WARNING) << "Failed to refresh token: " << e.what();
    }
  }
}

void token_manager::implementation::_request_login_notification() {
  LoginRequest request;
  request.set_service_name(SERVICE_NAME);
  google::protobuf::Empty response;
  grpc::ClientContext context;
  context.set_deadline(system_clock::now() + std::chrono::seconds(10));
  grpc::Status status = _auth_stub->RequestLogin(&context, request, &response);

  if (!status.ok()) {
    LOG(ERROR) << "gRPC RequestLogin failed: " << status.error_message();
  }
}

// MARK: Token Manager

void set_test_token_manager(token_manager& tm) {
  alternate_singleton = &tm;
}

void clear_test_token_manager() {
  alternate_singleton = nullptr;
}

token_manager& token_manager::get_instance() {
  if (alternate_singleton) return *alternate_singleton;
  static token_manager instance;
  return instance;
}

token_manager::token_manager()
    : _implementation(std::make_unique<implementation>()) {}

token_manager::token_manager(
    std::unique_ptr<AuthService::StubInterface> stub,
    database& db,
    std::unique_ptr<token_refresher> refresher)
    : _implementation(
          std::make_unique<implementation>(
              std::move(stub), db, std::move(refresher))) {}

token_manager::token_manager(
    defer_pump_start_t,
    std::unique_ptr<AuthService::StubInterface> stub,
    database& db,
    std::unique_ptr<token_refresher> refresher)
    : _implementation(
          std::make_unique<implementation>(
              defer_pump_start, std::move(stub), db, std::move(refresher))) {}

token_manager::~token_manager() = default;

std::string token_manager::get_bearer_token(
    bool clear_cache,
    std::chrono::milliseconds timeout,
    std::optional<std::reference_wrapper<const cancellation>> cancel) {
  if (is_cancelled(cancel)) {
    throw std::runtime_error("Cancelled while waiting for bearer token.");
  }

  std::string cache_token = _implementation->check_cache(clear_cache);
  if (!cache_token.empty()) return cache_token;

  cache_token = _implementation->wait_for(timeout, cancel);
  if (!cache_token.empty()) return cache_token;

  if (is_cancelled(cancel)) {
    throw std::runtime_error("Cancelled while waiting for bearer token.");
  }

  throw std::runtime_error("Timed out waiting for bearer token.");
}

void token_manager::start_pump() {
  // TODO: #111 - Create a way to prime the auth token at startup to ensure
  // the service fails-fast when something isn't right.
  _implementation->maybe_start_pump();
}

} // namespace howling

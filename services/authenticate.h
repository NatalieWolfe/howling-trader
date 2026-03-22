#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "api/schwab/oauth.h"
#include "services/database.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"

namespace howling {

class database;

struct defer_pump_start_t {
  explicit defer_pump_start_t() = default;
};
inline constexpr defer_pump_start_t defer_pump_start{};

class token_refresher {
public:
  virtual ~token_refresher() = default;
  virtual schwab::oauth_tokens
  refresh_tokens(std::string_view refresh_token) = 0;
};

class token_manager {
public:
  static token_manager& get_instance();

  token_manager(
      std::unique_ptr<AuthService::StubInterface> stub,
      database& db,
      std::unique_ptr<token_refresher> refresher);

  token_manager(
      defer_pump_start_t,
      std::unique_ptr<AuthService::StubInterface> stub,
      database& db,
      std::unique_ptr<token_refresher> refresher);

  ~token_manager();

  // Blocks until a valid token is available or a timeout occurs.
  std::string get_bearer_token(
      bool clear_cache = false,
      std::chrono::milliseconds timeout = std::chrono::minutes(5));

  void start_pump();

private:
  token_manager();
  token_manager(const token_manager&) = delete;
  token_manager& operator=(const token_manager&) = delete;

  struct implementation;
  std::unique_ptr<implementation> _implementation;
};

} // namespace howling

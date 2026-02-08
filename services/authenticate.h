#pragma once

#include <chrono>
#include <memory>
#include <string>

namespace howling {

class token_manager {
public:
  token_manager();
  ~token_manager();

  // Blocks until a valid token is available or a timeout occurs.
  std::string
  get_bearer_token(std::chrono::milliseconds timeout = std::chrono::minutes(5));

private:
  struct implementation;
  std::unique_ptr<implementation> _implementation;
};

} // namespace howling

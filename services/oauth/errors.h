#pragma once

#include <stdexcept>

namespace howling {

/**
 * @brief Thrown when an OAuth provider permanently rejects authentication
 * (e.g., HTTP 400 Bad Request or HTTP 401 Unauthorized with invalid_grant).
 */
class auth_rejected_error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

} // namespace howling

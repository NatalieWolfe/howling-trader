#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "services/security.h"
#include "services/security/certificate_bundle.h"
#include "gmock/gmock.h"

namespace howling {

class mock_security_client : public security_client {
public:
  MOCK_METHOD(
      void, wait_for_ready, (std::chrono::milliseconds timeout), (override));
  MOCK_METHOD(Json::Value, get_secret, (std::string_view path), (override));
  MOCK_METHOD(
      std::string,
      encrypt,
      (std::string_view key_name, std::string_view plaintext),
      (override));
  MOCK_METHOD(
      std::string,
      decrypt,
      (std::string_view key_name, std::string_view ciphertext),
      (override));
  MOCK_METHOD(
      certificate_bundle,
      create_certificate,
      (std::string_view role, std::string_view common_name),
      (override));
  MOCK_METHOD(std::string, get_ca_certificate, (), (override));
};

} // namespace howling

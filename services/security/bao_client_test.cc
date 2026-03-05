#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "services/security/bao_client.h"
#include "services/security/mock_bao_server.h"

ABSL_DECLARE_FLAG(std::string, bao_token_file);

namespace howling::security {

using ::testing::HasSubstr;
using ::testing::ThrowsMessage;

TEST(BaoClientLoginTest, Success) {
  // Create a temporary token file.
  std::string token_path = "test_token.txt";
  {
    std::ofstream token_file(token_path);
    token_file << "test_jwt_token";
  }
  absl::SetFlag(&FLAGS_bao_token_file, token_path);

  mock_bao_server server;
  server.start();

  bao_client client;
  EXPECT_NO_THROW(client.login());

  // Clean up.
  std::remove(token_path.c_str());
}

TEST(BaoClientWaitForReadyTest, ThrowsNotImplemented) {
  using namespace std::chrono_literals;
  bao_client client;
  EXPECT_THAT(
      [&] { client.wait_for_ready(100ms); },
      ThrowsMessage<std::runtime_error>(HasSubstr("Not implemented yet.")));
}

TEST(BaoClientLoginTest, ThrowsOnMissingTokenFile) {
  absl::SetFlag(&FLAGS_bao_token_file, "/non/existent/file");

  bao_client client;
  EXPECT_THAT(
      [&] { client.login(); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("Failed to open file: /non/existent/file")));
}

TEST(BaoClientGetSecretTest, ThrowsNotImplemented) {
  bao_client client;
  EXPECT_THAT(
      [&] { (void)client.get_secret("test/path"); },
      ThrowsMessage<std::runtime_error>(HasSubstr("Not implemented yet.")));
}

TEST(BaoClientEncryptTest, ThrowsNotImplemented) {
  bao_client client;
  EXPECT_THAT(
      [&] { (void)client.encrypt("test-key", "plaintext"); },
      ThrowsMessage<std::runtime_error>(HasSubstr("Not implemented yet.")));
}

TEST(BaoClientDecryptTest, ThrowsNotImplemented) {
  bao_client client;
  EXPECT_THAT(
      [&] { (void)client.decrypt("test-key", "ciphertext"); },
      ThrowsMessage<std::runtime_error>(HasSubstr("Not implemented yet.")));
}

} // namespace howling::security

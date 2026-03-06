#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "services/security/bao_client.h"
#include "services/security/mock_bao_server.h"

namespace howling::security {

using ::testing::HasSubstr;
using ::testing::ThrowsMessage;

TEST(BaoClientWaitForReadyTest, Success) {
  using namespace std::chrono_literals;
  mock_bao_server server(/*configure_flags=*/true, /*use_ssl=*/false);
  server.start();

  bao_client client;
  EXPECT_NO_THROW(client.wait_for_ready(1s));
}

TEST(BaoClientWaitForReadyTest, Timeout) {
  using namespace std::chrono_literals;
  // We don't start the server, so it should timeout.
  bao_client client;
  EXPECT_THAT(
      [&] { client.wait_for_ready(200ms); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("Timed out waiting for OpenBao proxy to be ready.")));
}

TEST(BaoClientGetSecretTest, Success) {
  mock_bao_server server(/*configure_flags=*/true, /*use_ssl=*/false);
  server.start();

  bao_client client;
  Json::Value secret = client.get_secret("howling/prod/telegram");
  EXPECT_EQ(secret["key"].asString(), "value");
}

TEST(BaoClientEncryptTest, Success) {
  mock_bao_server server(/*configure_flags=*/true, /*use_ssl=*/false);
  server.start();

  bao_client client;
  std::string ciphertext = client.encrypt("test-key", "test_plaintext");
  EXPECT_EQ(ciphertext, "vault:v1:test_ciphertext");
}

TEST(BaoClientDecryptTest, Success) {
  mock_bao_server server(/*configure_flags=*/true, /*use_ssl=*/false);
  server.start();

  bao_client client;
  std::string plaintext =
      client.decrypt("test-key", "vault:v1:test_ciphertext");
  EXPECT_EQ(plaintext, "test_plaintext");
}

} // namespace howling::security

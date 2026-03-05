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

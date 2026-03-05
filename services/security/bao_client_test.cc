#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "services/security/bao_client.h"

namespace howling::security {

using ::testing::HasSubstr;
using ::testing::ThrowsMessage;

TEST(BaoClientWaitForReadyTest, ThrowsNotImplemented) {
  using namespace std::chrono_literals;
  bao_client client;
  EXPECT_THAT(
      [&] { client.wait_for_ready(100ms); },
      ThrowsMessage<std::runtime_error>(HasSubstr("Not implemented yet.")));
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

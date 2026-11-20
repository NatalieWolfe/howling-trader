#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/flags/reflection.h"
#include "absl/time/time.h"
#include "functional/retry.h"
#include "services/security/bao_client.h"
#include "services/security/certificate_bundle.h"
#include "services/security/mock_bao_server.h"

ABSL_DECLARE_FLAG(absl::Duration, bao_secret_retry_timeout);

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

TEST(BaoClientGetSecretTest, RetriesOnNotFoundUntilAvailable) {
  mock_bao_server server(/*configure_flags=*/true, /*use_ssl=*/false);
  server.set_secret_not_found_count(2);
  server.start();

  bao_client client;
  Json::Value secret = client.get_secret("howling/prod/telegram");
  EXPECT_EQ(secret["key"].asString(), "value");
}

TEST(BaoClientGetSecretTest, ThrowsWhenNotFoundExceedsTimeout) {
  absl::FlagSaver flag_saver;
  absl::SetFlag(&FLAGS_bao_secret_retry_timeout, absl::Milliseconds(50));
  mock_bao_server server(/*configure_flags=*/true, /*use_ssl=*/false);
  server.set_secret_not_found_count(1000);
  server.start();

  bao_client client;
  EXPECT_THAT(
      [&] {
        [[maybe_unused]] Json::Value secret =
            client.get_secret("howling/prod/schwab");
      },
      ThrowsMessage<backoff_exceeded_error>(
          HasSubstr("Failed to retrieve secret via OpenBao: 404")));
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

TEST(BaoClientCreateCertificateTest, Success) {
  mock_bao_server server(/*configure_flags=*/true, /*use_ssl=*/false);
  server.start();

  bao_client client;
  certificate_bundle bundle =
      client.create_certificate("test-role", "test.example.com");

  EXPECT_EQ(bundle.certificate, "test_cert");
  EXPECT_EQ(bundle.private_key, "test_key");
  EXPECT_EQ(bundle.ca_chain, "test_cert\nca1\nca2\n");
}

TEST(BaoClientGetCaCertificateTest, Success) {
  mock_bao_server server(/*configure_flags=*/true, /*use_ssl=*/false);
  server.start();

  bao_client client;
  std::string ca_cert = client.get_ca_certificate();
  EXPECT_EQ(ca_cert, "test_ca_cert");
}

} // namespace howling::security

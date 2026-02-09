#include "services/db/crypto.h"

#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "gtest/gtest.h"

ABSL_DECLARE_FLAG(std::string, db_encryption_key);

namespace howling {
namespace {

class CryptoTest : public ::testing::Test {
protected:
  void SetUp() override {
    absl::SetFlag(
        &FLAGS_db_encryption_key,
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
  }
};

TEST_F(CryptoTest, RoundTrip) {
  std::string original = "my secret refresh token";
  std::string encrypted = encrypt_token(original);
  EXPECT_NE(original, encrypted);

  std::string decrypted = decrypt_token(encrypted);
  EXPECT_EQ(original, decrypted);
}

TEST_F(CryptoTest, UniqueNonces) {
  std::string original = "same content";
  std::string encrypted1 = encrypt_token(original);
  std::string encrypted2 = encrypt_token(original);

  // Even with same key and plaintext, ciphertext should be different due to
  // random nonce.
  EXPECT_NE(encrypted1, encrypted2);

  EXPECT_EQ(decrypt_token(encrypted1), original);
  EXPECT_EQ(decrypt_token(encrypted2), original);
}

TEST_F(CryptoTest, ThrowsOnInvalidKey) {
  absl::SetFlag(&FLAGS_db_encryption_key, "too-short");
  EXPECT_THROW(encrypt_token("test"), std::runtime_error);
}

TEST_F(CryptoTest, ThrowsOnCorruptedCiphertext) {
  std::string original = "test";
  std::string encrypted = encrypt_token(original);

  // Flip a bit in the ciphertext part (after the nonce).
  encrypted[encrypted.size() - 1] ^= 0x01;

  EXPECT_THROW(decrypt_token(encrypted), std::runtime_error);
}

TEST_F(CryptoTest, ThrowsOnTruncatedInput) {
  EXPECT_THROW(decrypt_token("short"), std::runtime_error);
}

} // namespace
} // namespace howling

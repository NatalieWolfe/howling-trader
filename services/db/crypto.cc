#include "services/db/crypto.h"

#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/escaping.h"
#include "openssl/aead.h"
#include "openssl/rand.h"

ABSL_FLAG(
    std::string,
    db_encryption_key,
    "",
    "32-byte hex-encoded key for database encryption.");

namespace howling {
namespace {

constexpr size_t KEY_SIZE = 32;
constexpr size_t NONCE_SIZE = 12;

std::string get_key() {
  std::string hex_key = absl::GetFlag(FLAGS_db_encryption_key);
  if (hex_key.empty()) {
    throw std::runtime_error("Database encryption key not set.");
  }
  std::string key = absl::HexStringToBytes(hex_key);
  if (key.size() != KEY_SIZE) {
    throw std::runtime_error("Database encryption key must be 32 bytes.");
  }
  return key;
}

} // namespace

std::string encrypt_token(std::string_view plaintext) {
  std::string key = get_key();
  std::string nonce(NONCE_SIZE, '\0');
  RAND_bytes(reinterpret_cast<uint8_t*>(nonce.data()), NONCE_SIZE);

  EVP_AEAD_CTX ctx;
  if (!EVP_AEAD_CTX_init(
          &ctx,
          EVP_aead_aes_256_gcm(),
          reinterpret_cast<const uint8_t*>(key.data()),
          key.size(),
          EVP_AEAD_DEFAULT_TAG_LENGTH,
          nullptr)) {
    throw std::runtime_error("Failed to initialize encryption context.");
  }

  size_t max_out_len =
      plaintext.size() + EVP_AEAD_max_overhead(EVP_aead_aes_256_gcm());
  std::string ciphertext(max_out_len, '\0');
  size_t out_len;

  if (!EVP_AEAD_CTX_seal(
          &ctx,
          reinterpret_cast<uint8_t*>(ciphertext.data()),
          &out_len,
          max_out_len,
          reinterpret_cast<const uint8_t*>(nonce.data()),
          nonce.size(),
          reinterpret_cast<const uint8_t*>(plaintext.data()),
          plaintext.size(),
          nullptr,
          0)) {
    EVP_AEAD_CTX_cleanup(&ctx);
    throw std::runtime_error("Encryption failed.");
  }
  EVP_AEAD_CTX_cleanup(&ctx);

  ciphertext.resize(out_len);
  return absl::StrCat(nonce, ciphertext);
}

std::string decrypt_token(std::string_view input) {
  std::string key = get_key();
  if (input.size() < NONCE_SIZE) {
    throw std::runtime_error("Invalid ciphertext: too short.");
  }

  std::string_view nonce = input.substr(0, NONCE_SIZE);
  std::string_view ciphertext = input.substr(NONCE_SIZE);

  EVP_AEAD_CTX ctx;
  if (!EVP_AEAD_CTX_init(
          &ctx,
          EVP_aead_aes_256_gcm(),
          reinterpret_cast<const uint8_t*>(key.data()),
          key.size(),
          EVP_AEAD_DEFAULT_TAG_LENGTH,
          nullptr)) {
    throw std::runtime_error("Failed to initialize decryption context.");
  }

  std::string plaintext(ciphertext.size(), '\0');
  size_t out_len;

  if (!EVP_AEAD_CTX_open(
          &ctx,
          reinterpret_cast<uint8_t*>(plaintext.data()),
          &out_len,
          plaintext.size(),
          reinterpret_cast<const uint8_t*>(nonce.data()),
          nonce.size(),
          reinterpret_cast<const uint8_t*>(ciphertext.data()),
          ciphertext.size(),
          nullptr,
          0)) {
    EVP_AEAD_CTX_cleanup(&ctx);
    throw std::runtime_error("Decryption failed.");
  }
  EVP_AEAD_CTX_cleanup(&ctx);

  plaintext.resize(out_len);
  return plaintext;
}

} // namespace howling

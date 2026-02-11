#include "services/db/crypto.h"

#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "openssl/evp.h"
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
constexpr size_t TAG_SIZE = 16;

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

struct cipher_context {
  cipher_context() : ctx(EVP_CIPHER_CTX_new()) {
    if (!ctx) throw std::runtime_error("Failed to create cipher context.");
  }
  ~cipher_context() { EVP_CIPHER_CTX_free(ctx); }
  operator EVP_CIPHER_CTX*() { return ctx; }

  EVP_CIPHER_CTX* ctx;
};

} // namespace

std::string encrypt_token(std::string_view plaintext) {
  std::string key = get_key();
  std::string nonce(NONCE_SIZE, '\0');
  if (RAND_bytes(reinterpret_cast<uint8_t*>(nonce.data()), NONCE_SIZE) != 1) {
    throw std::runtime_error("Failed to generate random nonce.");
  }

  cipher_context ctx;
  if (EVP_EncryptInit_ex(
          ctx,
          EVP_aes_256_gcm(),
          nullptr,
          reinterpret_cast<const uint8_t*>(key.data()),
          reinterpret_cast<const uint8_t*>(nonce.data())) != 1) {
    throw std::runtime_error("Failed to initialize encryption.");
  }

  std::string ciphertext(plaintext.size(), '\0');
  int len;
  if (EVP_EncryptUpdate(
          ctx,
          reinterpret_cast<uint8_t*>(ciphertext.data()),
          &len,
          reinterpret_cast<const uint8_t*>(plaintext.data()),
          plaintext.size()) != 1) {
    throw std::runtime_error("Encryption update failed.");
  }

  if (EVP_EncryptFinal_ex(ctx, nullptr, &len) != 1) {
    throw std::runtime_error("Encryption finalization failed.");
  }

  std::string tag(TAG_SIZE, '\0');
  if (EVP_CIPHER_CTX_ctrl(
          ctx,
          EVP_CTRL_GCM_GET_TAG,
          TAG_SIZE,
          reinterpret_cast<uint8_t*>(tag.data())) != 1) {
    throw std::runtime_error("Failed to retrieve authentication tag.");
  }

  return absl::StrCat(nonce, ciphertext, tag);
}

std::string decrypt_token(std::string_view input) {
  if (input.size() < NONCE_SIZE + TAG_SIZE) {
    throw std::runtime_error("Invalid ciphertext: too short.");
  }

  std::string key = get_key();
  std::string_view nonce = input.substr(0, NONCE_SIZE);
  std::string_view tag = input.substr(input.size() - TAG_SIZE);
  std::string_view ciphertext =
      input.substr(NONCE_SIZE, input.size() - NONCE_SIZE - TAG_SIZE);

  cipher_context ctx;
  if (EVP_DecryptInit_ex(
          ctx,
          EVP_aes_256_gcm(),
          nullptr,
          reinterpret_cast<const uint8_t*>(key.data()),
          reinterpret_cast<const uint8_t*>(nonce.data())) != 1) {
    throw std::runtime_error("Failed to initialize decryption.");
  }

  std::string plaintext(ciphertext.size(), '\0');
  int len;
  if (EVP_DecryptUpdate(
          ctx,
          reinterpret_cast<uint8_t*>(plaintext.data()),
          &len,
          reinterpret_cast<const uint8_t*>(ciphertext.data()),
          ciphertext.size()) != 1) {
    throw std::runtime_error("Decryption update failed.");
  }

  if (EVP_CIPHER_CTX_ctrl(
          ctx,
          EVP_CTRL_GCM_SET_TAG,
          TAG_SIZE,
          const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(tag.data()))) !=
      1) {
    throw std::runtime_error("Failed to set authentication tag.");
  }

  if (EVP_DecryptFinal_ex(ctx, nullptr, &len) != 1) {
    throw std::runtime_error("Decryption authentication failed.");
  }

  return plaintext;
}

} // namespace howling

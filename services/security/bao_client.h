#pragma once

#include <string>
#include <string_view>

#include "json/value.h"

namespace howling::security {

/**
 * @brief Client for interacting with the OpenBao API.
 *
 * This client provides methods for authenticating with OpenBao using Kubernetes
 * ServiceAccount tokens, retrieving secrets from the KV-v2 engine, and
 * performing encryption/decryption operations using the Transit engine.
 */
class bao_client {
public:
  bao_client();

  /**
   * @brief Exchanges a Kubernetes ServiceAccount token for an OpenBao token.
   *
   * @throws std::runtime_error on failure.
   */
  void login();

  /**
   * @brief Retrieves a secret from the KV-v2 engine.
   *
   * @param path The path to the secret (e.g., "howling/prod/telegram").
   * @return The secret data as a Json::Value.
   *
   * @throws std::runtime_error on failure.
   */
  [[nodiscard]] Json::Value get_secret(std::string_view path);

  /**
   * @brief Encrypts data using the Transit engine.
   *
   * @param key_name The name of the encryption key in the Transit engine.
   * @param plaintext The data to encrypt.
   * @return The ciphertext returned by OpenBao.
   *
   * @throws std::runtime_error on failure.
   */
  [[nodiscard]] std::string
  encrypt(std::string_view key_name, std::string_view plaintext);

  /**
   * @brief Decrypts data using the Transit engine.
   *
   * @param key_name The name of the encryption key in the Transit engine.
   * @param ciphertext The data to decrypt.
   * @return The decrypted plaintext.
   *
   * @throws std::runtime_error on failure.
   */
  [[nodiscard]] std::string
  decrypt(std::string_view key_name, std::string_view ciphertext);

private:
  std::string _token;
};

} // namespace howling::security

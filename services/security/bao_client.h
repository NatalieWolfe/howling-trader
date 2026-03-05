#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "json/value.h"

namespace howling::security {

/**
 * @brief Client for interacting with the OpenBao API.
 *
 * This client provides methods for interacting with the OpenBao Proxy sidecar,
 * retrieving secrets from the KV-v2 engine, and performing
 * encryption/decryption operations using the Transit engine.
 */
class bao_client {
public:
  bao_client();

  /**
   * @brief Blocks until the OpenBao proxy is ready to handle requests.
   *
   * This method probes the OpenBao health endpoint and retries with exponential
   * backoff until the proxy is unsealed and ready or the timeout is reached.
   *
   * @param timeout The maximum amount of time to wait.
   *
   * @throws std::runtime_error if the timeout is reached before the proxy is
   * ready.
   */
  void wait_for_ready(std::chrono::milliseconds timeout);

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
};

} // namespace howling::security

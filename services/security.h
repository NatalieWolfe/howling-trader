#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "services/security/certificate_bundle.h"
#include "services/service_base.h"
#include "json/value.h"

namespace howling {

/**
 * @brief Abstract interface for security operations.
 *
 * This interface provides methods for interacting with a security backend
 * (like OpenBao) for secret retrieval and encryption/decryption.
 */
class security_client : public service {
public:
  virtual ~security_client() = default;

  /**
   * @brief Blocks until the security backend is ready to handle requests.
   *
   * @param timeout The maximum amount of time to wait.
   *
   * @throws std::runtime_error if the timeout is reached before the backend is
   * ready.
   */
  virtual void wait_for_ready(std::chrono::milliseconds timeout) = 0;

  /**
   * @brief Retrieves a secret from the security backend.
   *
   * @param path The path to the secret.
   * @return The secret data as a Json::Value.
   *
   * @throws std::runtime_error on failure.
   */
  virtual Json::Value get_secret(std::string_view path) = 0;

  /**
   * @brief Encrypts data using the security backend.
   *
   * @param key_name The name of the encryption key to use.
   * @param plaintext The data to encrypt.
   * @return The ciphertext.
   *
   * @throws std::runtime_error on failure.
   */
  virtual std::string
  encrypt(std::string_view key_name, std::string_view plaintext) = 0;

  /**
   * @brief Decrypts data using the security backend.
   *
   * @param key_name The name of the encryption key to use.
   * @param ciphertext The data to decrypt.
   * @return The decrypted plaintext.
   *
   * @throws std::runtime_error on failure.
   */
  virtual std::string
  decrypt(std::string_view key_name, std::string_view ciphertext) = 0;

  /**
   * @brief Creates a new certificate for the given service role.
   *
   * @param role
   * @param common_name
   * @return The newly created certificate bundle.
   */
  virtual certificate_bundle
  create_certificate(std::string_view role, std::string_view common_name) = 0;

  /**
   * @brief Retrieves the root certificate in PEM format.
   *
   * @return The root cert.
   */
  virtual std::string get_ca_certificate() = 0;
};

} // namespace howling

#pragma once

#include <string>
#include <string_view>

namespace howling {

/**
 * @brief Encrypts the plaintext using AES-256-GCM.
 *
 * @return A concatenations of the nonce with the ciphertext and tag.
 *
 * @throws std::runtime_error on failure.
 */
std::string encrypt_token(std::string_view plaintext);

/**
 * @brief Decrypts the ciphertext using AES-256-GCM.
 *
 * Expects the input to contain the nonce followed by the ciphertext and tag.
 *
 * @throws std::runtime_error on failure.
 */
std::string decrypt_token(std::string_view ciphertext);

} // namespace howling

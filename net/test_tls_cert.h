#pragma once

#include <string>

namespace howling::net::testing {

/**
 * @brief Holds a PEM-encoded self-signed TLS certificate and private key pair.
 */
struct self_signed_cert {
  std::string cert_pem;
  std::string key_pem;
};

/**
 * @brief Generates an ephemeral self-signed ECDSA TLS certificate for local
 * loopback tests.
 */
self_signed_cert generate_self_signed_cert();

} // namespace howling::net::testing

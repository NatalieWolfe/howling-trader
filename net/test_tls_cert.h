#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "openssl/bio.h"
#include "openssl/ec.h"
#include "openssl/evp.h"
#include "openssl/obj_mac.h"
#include "openssl/pem.h"
#include "openssl/x509.h"
#include "openssl/x509v3.h"

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
inline self_signed_cert generate_self_signed_cert() {
  std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)> ec_key{
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1), &EC_KEY_free};
  if (!ec_key || EC_KEY_generate_key(ec_key.get()) != 1) {
    throw std::runtime_error("Failed to generate EC key for test certificate.");
  }

  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey{
      EVP_PKEY_new(), &EVP_PKEY_free};
  if (!pkey || EVP_PKEY_assign_EC_KEY(pkey.get(), ec_key.get()) != 1) {
    throw std::runtime_error("Failed to assign EC key to EVP_PKEY.");
  }
  ec_key.release();

  std::unique_ptr<X509, decltype(&X509_free)> x509{X509_new(), &X509_free};
  if (!x509) {
    throw std::runtime_error("Failed to allocate X509 certificate.");
  }
  X509_set_version(x509.get(), 2);
  ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), 1);
  X509_gmtime_adj(X509_get_notBefore(x509.get()), -60);
  X509_gmtime_adj(X509_get_notAfter(x509.get()), 3600);
  X509_set_pubkey(x509.get(), pkey.get());

  X509_NAME* subject_name = X509_get_subject_name(x509.get());
  X509_NAME_add_entry_by_txt(
      subject_name,
      "CN",
      MBSTRING_ASC,
      reinterpret_cast<const unsigned char*>("127.0.0.1"),
      -1,
      -1,
      0);
  X509_set_issuer_name(x509.get(), subject_name);

  std::unique_ptr<BASIC_CONSTRAINTS, decltype(&BASIC_CONSTRAINTS_free)>
      basic_constraints{BASIC_CONSTRAINTS_new(), &BASIC_CONSTRAINTS_free};
  if (!basic_constraints) {
    throw std::runtime_error("Failed to allocate Basic Constraints.");
  }
  basic_constraints->ca = 1;
  if (X509_add1_ext_i2d(
          x509.get(),
          NID_basic_constraints,
          basic_constraints.get(),
          /*crit=*/1,
          /*flags=*/0) != 1) {
    throw std::runtime_error("Failed to add Basic Constraints extension.");
  }

  if (X509_sign(x509.get(), pkey.get(), EVP_sha256()) == 0) {
    throw std::runtime_error("Failed to sign test X509 certificate.");
  }

  std::unique_ptr<BIO, decltype(&BIO_free)> cert_bio{
      BIO_new(BIO_s_mem()), &BIO_free};
  std::unique_ptr<BIO, decltype(&BIO_free)> key_bio{
      BIO_new(BIO_s_mem()), &BIO_free};
  if (!cert_bio || !key_bio ||
      PEM_write_bio_X509(cert_bio.get(), x509.get()) != 1 ||
      PEM_write_bio_PrivateKey(
          key_bio.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr) !=
          1) {
    throw std::runtime_error("Failed to serialize test certificate to PEM.");
  }

  char* cert_data = nullptr;
  long cert_len = BIO_get_mem_data(cert_bio.get(), &cert_data);
  char* key_data = nullptr;
  long key_len = BIO_get_mem_data(key_bio.get(), &key_data);

  return self_signed_cert{
      .cert_pem = std::string(cert_data, static_cast<size_t>(cert_len)),
      .key_pem = std::string(key_data, static_cast<size_t>(key_len)),
  };
}

} // namespace howling::net::testing

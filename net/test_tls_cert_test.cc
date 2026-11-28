#include "net/test_tls_cert.h"

#include <memory>
#include <string_view>

#include "openssl/bio.h"
#include "openssl/evp.h"
#include "openssl/pem.h"
#include "openssl/x509.h"
#include "openssl/x509v3.h"
#include "gtest/gtest.h"

namespace howling::net::testing {
namespace {

using bio_ptr = std::unique_ptr<BIO, decltype(&BIO_free)>;
using evp_pkey_ptr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using x509_ptr = std::unique_ptr<X509, decltype(&X509_free)>;

x509_ptr parse_certificate(std::string_view cert_pem) {
  bio_ptr cert_bio{
      BIO_new_mem_buf(cert_pem.data(), static_cast<int>(cert_pem.size())),
      &BIO_free};
  if (!cert_bio) return x509_ptr{nullptr, &X509_free};
  return x509_ptr{
      PEM_read_bio_X509(cert_bio.get(), nullptr, nullptr, nullptr), &X509_free};
}

evp_pkey_ptr parse_private_key(std::string_view key_pem) {
  bio_ptr key_bio{
      BIO_new_mem_buf(key_pem.data(), static_cast<int>(key_pem.size())),
      &BIO_free};
  if (!key_bio) return evp_pkey_ptr{nullptr, &EVP_PKEY_free};
  return evp_pkey_ptr{
      PEM_read_bio_PrivateKey(key_bio.get(), nullptr, nullptr, nullptr),
      &EVP_PKEY_free};
}

TEST(TestTlsCertTest, GeneratesValidKeypairAndSelfSignedCertificate) {
  self_signed_cert cert = generate_self_signed_cert();
  ASSERT_FALSE(cert.cert_pem.empty());
  ASSERT_FALSE(cert.key_pem.empty());

  x509_ptr certificate = parse_certificate(cert.cert_pem);
  ASSERT_NE(certificate, nullptr);

  evp_pkey_ptr private_key = parse_private_key(cert.key_pem);
  ASSERT_NE(private_key, nullptr);

  EXPECT_EQ(X509_check_private_key(certificate.get(), private_key.get()), 1);
  EXPECT_EQ(X509_verify(certificate.get(), private_key.get()), 1);
}

TEST(TestTlsCertTest, IncludesLoopbackSubjectAlternativeNames) {
  self_signed_cert cert = generate_self_signed_cert();
  x509_ptr certificate = parse_certificate(cert.cert_pem);
  ASSERT_NE(certificate, nullptr);

  constexpr std::string_view LOCALHOST = "localhost";
  EXPECT_EQ(
      X509_check_host(
          certificate.get(),
          LOCALHOST.data(),
          LOCALHOST.size(),
          X509_CHECK_FLAG_NEVER_CHECK_SUBJECT,
          nullptr),
      1);
  EXPECT_EQ(X509_check_ip_asc(certificate.get(), "127.0.0.1", /*flags=*/0), 1);

  constexpr std::string_view UNLISTED_HOST = "example.invalid";
  EXPECT_EQ(
      X509_check_host(
          certificate.get(),
          UNLISTED_HOST.data(),
          UNLISTED_HOST.size(),
          X509_CHECK_FLAG_NEVER_CHECK_SUBJECT,
          nullptr),
      0);
  EXPECT_EQ(X509_check_ip_asc(certificate.get(), "127.0.0.2", /*flags=*/0), 0);
}

} // namespace
} // namespace howling::net::testing

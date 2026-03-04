#include "services/security/bao_client.h"

#include <stdexcept>
#include <string_view>

namespace howling::security {

bao_client::bao_client() {}

void bao_client::login() {
  throw std::runtime_error("Not implemented yet.");
}

Json::Value bao_client::get_secret(std::string_view path) {
  throw std::runtime_error("Not implemented yet.");
}

std::string
bao_client::encrypt(std::string_view key_name, std::string_view plaintext) {
  throw std::runtime_error("Not implemented yet.");
}

std::string
bao_client::decrypt(std::string_view key_name, std::string_view ciphertext) {
  throw std::runtime_error("Not implemented yet.");
}

} // namespace howling::security

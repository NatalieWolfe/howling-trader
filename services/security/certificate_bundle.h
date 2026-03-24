#pragma once

#include <string>

namespace howling {

struct certificate_bundle {
  std::string certificate;
  std::string private_key;
  std::string ca_chain;
};

} // namespace howling

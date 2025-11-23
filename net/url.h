#pragma once

#include <string>

namespace howling::net {

struct url {
  std::string service;
  std::string host;
  std::string target;
};

} // namespace howling::net

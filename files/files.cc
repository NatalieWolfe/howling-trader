#include "files/files.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

#include "absl/strings/str_cat.h"

namespace howling::files {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error(
        absl::StrCat("Failed to open file: ", path.string()));
  }

  return std::string(
      (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

} // namespace howling::files

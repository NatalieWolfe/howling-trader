#include "files/files.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"

namespace howling::files {

namespace fs = ::std::filesystem;

std::string read_file(const fs::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error(
        absl::StrCat("Failed to open file: ", path.string()));
  }

  return std::string(
      (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void write_file(const fs::path& path, std::string_view content) {
  std::ofstream file(path, std::ios::trunc);
  if (!file.is_open()) {
    throw std::runtime_error(
        absl::StrCat("Failed to open file for write: ", path.string()));
  }
  file << content << std::flush;
}

} // namespace howling::files

#pragma once

#include <filesystem>
#include <string>

namespace howling::files {

/**
 * @brief Reads the entire content of a file into a string.
 *
 * @param path The path to the file.
 * @return The content of the file.
 *
 * @throws std::runtime_error if the file cannot be opened.
 */
std::string read_file(const std::filesystem::path& path);

} // namespace howling::files

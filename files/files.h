#pragma once

#include <filesystem>
#include <string>
#include <string_view>

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

/**
 * @brief Writes the given content to the file specified.
 *
 * Upon completion the file will exist and its content will be exactly that
 * given.
 *
 * @param path    The file to write to.
 * @param content The content to put in the file.
 */
void write_file(const std::filesystem::path& path, std::string_view content);

} // namespace howling::files

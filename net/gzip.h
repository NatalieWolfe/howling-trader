#pragma once

#include <string>
#include <string_view>

namespace howling::net {

/**
 * @brief Decompresses a byte stream encoded in gzip (RFC 1952) or zlib/deflate
 * (RFC 1950) format.
 *
 * @param compressed_data The compressed byte stream.
 *
 * @throws std::runtime_error If the data is corrupted, truncated, or invalid.
 *
 * @return The decompressed uncompressed data.
 */
[[nodiscard]] std::string gzip_decompress(std::string_view compressed_data);

/**
 * @brief Compresses uncompressed data into a gzip-encoded byte stream (RFC
 * 1952).
 *
 * @param uncompressed_data The data to compress.
 *
 * @throws std::runtime_error If compression fails.
 *
 * @return The gzip-compressed byte stream.
 */
[[nodiscard]] std::string gzip_compress(std::string_view uncompressed_data);

/**
 * @brief Checks if the data begins with the gzip magic byte signature (0x1f,
 * 0x8b).
 *
 * @param data The byte stream to inspect.
 *
 * @return True if the data starts with gzip magic bytes.
 */
[[nodiscard]] bool is_gzip_content(std::string_view data);

/**
 * @brief Checks if an HTTP Content-Encoding or Transfer-Encoding value
 * indicates gzip or deflate compression.
 *
 * Checks case-insensitively for "gzip", "x-gzip", or "deflate".
 *
 * @param encoding The encoding header value to check.
 *
 * @return True if the encoding represents a supported compression format.
 */
[[nodiscard]] bool is_compressed_encoding(std::string_view encoding);

} // namespace howling::net

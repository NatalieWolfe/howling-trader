#include "net/gzip.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

#include "zlib.h"

namespace howling::net {
namespace {

// Offsets defined by zlib specification for windowBits configuration:
// - Adding 16 enables gzip format only (RFC 1952).
// - Adding 32 enables automatic detection and decoding of both zlib (RFC 1950)
//   and gzip (RFC 1952).
constexpr int GZIP_WINDOW_BITS_OFFSET = 16;
constexpr int AUTO_DETECT_WINDOW_BITS_OFFSET = 32;

constexpr int GZIP_ENCODING_WINDOW_BITS = MAX_WBITS + GZIP_WINDOW_BITS_OFFSET;
constexpr int AUTO_DETECT_WINDOW_BITS =
    MAX_WBITS + AUTO_DETECT_WINDOW_BITS_OFFSET;
constexpr int DEFAULT_MEM_LEVEL = 8;
constexpr std::size_t DECOMPRESSION_CHUNK_SIZE = 16384;
constexpr uint8_t GZIP_MAGIC_BYTE_FIRST = 0x1f;
constexpr uint8_t GZIP_MAGIC_BYTE_SECOND = 0x8b;

struct inflate_guard {
  z_stream& stream;
  ~inflate_guard() { inflateEnd(&stream); }
};

struct deflate_guard {
  z_stream& stream;
  ~deflate_guard() { deflateEnd(&stream); }
};

bool contains_case_insensitive(
    std::string_view text, std::string_view pattern) {
  auto to_lower = [](char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  };
  return !std::ranges::search(text, pattern, {}, to_lower, to_lower).empty();
}

} // namespace

std::string gzip_decompress(std::string_view compressed_data) {
  if (compressed_data.empty()) return "";

  z_stream stream{};
  int ret = inflateInit2(&stream, AUTO_DETECT_WINDOW_BITS);
  if (ret != Z_OK) {
    throw std::runtime_error(
        std::format(
            "Failed to initialize zlib inflate: [{}] {}", ret, zError(ret)));
  }
  inflate_guard guard{stream};

  stream.avail_in = compressed_data.size();
  stream.next_in = const_cast<Bytef*>(
      reinterpret_cast<const Bytef*>(compressed_data.data()));

  std::string decompressed;
  char chunk[DECOMPRESSION_CHUNK_SIZE];

  do {
    stream.avail_out = sizeof(chunk);
    stream.next_out = reinterpret_cast<Bytef*>(chunk);

    ret = inflate(&stream, Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
      const char* message = stream.msg != nullptr ? stream.msg : zError(ret);
      throw std::runtime_error(
          std::format("zlib inflate error [{}]: {}", ret, message));
    }

    decompressed.append(chunk, sizeof(chunk) - stream.avail_out);
  } while (ret != Z_STREAM_END && stream.avail_in > 0);

  if (ret != Z_STREAM_END) {
    throw std::runtime_error(
        "Unexpected end of compressed data: truncated stream.");
  }

  return decompressed;
}

std::string gzip_compress(std::string_view uncompressed_data) {
  z_stream stream{};
  int ret = deflateInit2(
      &stream,
      Z_DEFAULT_COMPRESSION,
      Z_DEFLATED,
      GZIP_ENCODING_WINDOW_BITS,
      DEFAULT_MEM_LEVEL,
      Z_DEFAULT_STRATEGY);
  if (ret != Z_OK) {
    throw std::runtime_error(
        std::format(
            "Failed to initialize zlib deflate: [{}] {}", ret, zError(ret)));
  }
  deflate_guard guard{stream};

  stream.avail_in = uncompressed_data.size();
  stream.next_in = const_cast<Bytef*>(
      reinterpret_cast<const Bytef*>(uncompressed_data.data()));

  std::string compressed;
  char chunk[DECOMPRESSION_CHUNK_SIZE];

  do {
    stream.avail_out = sizeof(chunk);
    stream.next_out = reinterpret_cast<Bytef*>(chunk);

    ret = deflate(&stream, Z_FINISH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
      const char* message = stream.msg != nullptr ? stream.msg : zError(ret);
      throw std::runtime_error(
          std::format("zlib deflate error [{}]: {}", ret, message));
    }

    compressed.append(chunk, sizeof(chunk) - stream.avail_out);
  } while (ret != Z_STREAM_END);

  return compressed;
}

bool is_gzip_content(std::string_view data) {
  return data.size() >= 2 &&
      static_cast<uint8_t>(data[0]) == GZIP_MAGIC_BYTE_FIRST &&
      static_cast<uint8_t>(data[1]) == GZIP_MAGIC_BYTE_SECOND;
}

bool is_compressed_encoding(std::string_view encoding) {
  if (encoding.empty()) return false;
  return contains_case_insensitive(encoding, "gzip") ||
      contains_case_insensitive(encoding, "deflate");
}

} // namespace howling::net

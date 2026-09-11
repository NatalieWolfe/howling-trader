#include "net/gzip.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include <zlib.h>

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
  z_stream& strm;
  ~inflate_guard() {
    inflateEnd(&strm);
  }
};

struct deflate_guard {
  z_stream& strm;
  ~deflate_guard() {
    deflateEnd(&strm);
  }
};

} // namespace

std::string gzip_decompress(std::string_view compressed_data) {
  if (compressed_data.empty()) {
    return "";
  }

  z_stream strm{};
  int ret = inflateInit2(&strm, AUTO_DETECT_WINDOW_BITS);
  if (ret != Z_OK) {
    throw std::runtime_error(
        absl::StrCat("Failed to initialize zlib inflate: ", ret));
  }
  inflate_guard guard{strm};

  strm.avail_in = compressed_data.size();
  strm.next_in = const_cast<Bytef*>(
      reinterpret_cast<const Bytef*>(compressed_data.data()));

  std::string decompressed;
  char chunk[DECOMPRESSION_CHUNK_SIZE];

  do {
    strm.avail_out = sizeof(chunk);
    strm.next_out = reinterpret_cast<Bytef*>(chunk);

    ret = inflate(&strm, Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
      const char* msg = strm.msg != nullptr ? strm.msg : "unknown error";
      throw std::runtime_error(
          absl::StrCat("zlib inflate error (", ret, "): ", msg));
    }

    decompressed.append(chunk, sizeof(chunk) - strm.avail_out);
  } while (ret != Z_STREAM_END && strm.avail_in > 0);

  if (ret != Z_STREAM_END) {
    throw std::runtime_error(
        "Unexpected end of compressed data: truncated stream.");
  }

  return decompressed;
}

std::string gzip_compress(std::string_view uncompressed_data) {
  z_stream strm{};
  int ret = deflateInit2(
      &strm,
      Z_DEFAULT_COMPRESSION,
      Z_DEFLATED,
      GZIP_ENCODING_WINDOW_BITS,
      DEFAULT_MEM_LEVEL,
      Z_DEFAULT_STRATEGY);
  if (ret != Z_OK) {
    throw std::runtime_error(
        absl::StrCat("Failed to initialize zlib deflate: ", ret));
  }
  deflate_guard guard{strm};

  strm.avail_in = uncompressed_data.size();
  strm.next_in = const_cast<Bytef*>(
      reinterpret_cast<const Bytef*>(uncompressed_data.data()));

  std::string compressed;
  char chunk[DECOMPRESSION_CHUNK_SIZE];

  do {
    strm.avail_out = sizeof(chunk);
    strm.next_out = reinterpret_cast<Bytef*>(chunk);

    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
      const char* msg = strm.msg != nullptr ? strm.msg : "unknown error";
      throw std::runtime_error(
          absl::StrCat("zlib deflate error (", ret, "): ", msg));
    }

    compressed.append(chunk, sizeof(chunk) - strm.avail_out);
  } while (ret != Z_STREAM_END);

  return compressed;
}

bool is_gzip_content(std::string_view data) {
  return data.size() >= 2 &&
         static_cast<uint8_t>(data[0]) == GZIP_MAGIC_BYTE_FIRST &&
         static_cast<uint8_t>(data[1]) == GZIP_MAGIC_BYTE_SECOND;
}

bool is_compressed_encoding(std::string_view encoding) {
  if (encoding.empty()) {
    return false;
  }
  std::string lower;
  lower.reserve(encoding.size());
  for (char c : encoding) {
    lower.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return lower.find("gzip") != std::string::npos ||
         lower.find("deflate") != std::string::npos;
}

} // namespace howling::net

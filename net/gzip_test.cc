#include "net/gzip.h"

#include <string>
#include <string_view>

#include "zlib.h"
#include "gtest/gtest.h"

namespace howling::net {
namespace {

TEST(GzipTest, EmptyStringDecompress) {
  EXPECT_EQ(gzip_decompress(""), "");
}

TEST(GzipTest, EmptyStringRoundtrip) {
  std::string compressed = gzip_compress("");
  EXPECT_TRUE(is_gzip_content(compressed));
  EXPECT_EQ(gzip_decompress(compressed), "");
}

TEST(GzipTest, SimpleRoundtrip) {
  const std::string original =
      R"({"symbol":"AAPL","candles":[{"open":150.0,"close":155.0}]})";
  std::string compressed = gzip_compress(original);

  EXPECT_TRUE(is_gzip_content(compressed));
  EXPECT_NE(compressed, original);

  std::string decompressed = gzip_decompress(compressed);
  EXPECT_EQ(decompressed, original);
}

TEST(GzipTest, LargePayloadRoundtrip) {
  std::string original;
  original.reserve(250000);
  for (int i = 0; i < 5000; ++i) {
    original.append(R"({"datetime":1630000000,"open":100.5,"close":104.1},)");
  }

  std::string compressed = gzip_compress(original);
  EXPECT_TRUE(is_gzip_content(compressed));
  EXPECT_LT(compressed.size(), original.size());

  std::string decompressed = gzip_decompress(compressed);
  EXPECT_EQ(decompressed, original);
}

TEST(GzipTest, CorruptedDataThrows) {
  const std::string invalid_data = "This is not gzip compressed data at all!";
  EXPECT_THROW((void)gzip_decompress(invalid_data), std::runtime_error);
}

TEST(GzipTest, TruncatedDataThrows) {
  const std::string original =
      "Some text that will be compressed and truncated.";
  std::string compressed = gzip_compress(original);
  ASSERT_GT(compressed.size(), 10u);

  std::string truncated = compressed.substr(0, compressed.size() - 8);
  EXPECT_THROW((void)gzip_decompress(truncated), std::runtime_error);
}

TEST(GzipTest, IsGzipContentDetection) {
  EXPECT_FALSE(is_gzip_content(""));
  EXPECT_FALSE(is_gzip_content("\x1f"));
  EXPECT_FALSE(is_gzip_content("random string"));
  EXPECT_TRUE(is_gzip_content("\x1f\x8b\x08\x00\x00\x00\x00\x00"));
}

TEST(GzipTest, IsCompressedEncodingDetection) {
  EXPECT_FALSE(is_compressed_encoding(""));
  EXPECT_FALSE(is_compressed_encoding("identity"));
  EXPECT_FALSE(is_compressed_encoding("br"));

  EXPECT_TRUE(is_compressed_encoding("gzip"));
  EXPECT_TRUE(is_compressed_encoding("GZIP"));
  EXPECT_TRUE(is_compressed_encoding("x-gzip"));
  EXPECT_TRUE(is_compressed_encoding("deflate"));
  EXPECT_TRUE(is_compressed_encoding("gzip, deflate"));
  EXPECT_TRUE(is_compressed_encoding("deflate, gzip"));
}

TEST(GzipTest, DecompressZlibFormat) {
  // Test that RFC 1950 zlib-wrapped deflate streams are also decompressed.
  const std::string original = "Decompress me from RFC 1950 zlib format!";
  z_stream stream{};
  int ret = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
  ASSERT_EQ(ret, Z_OK);

  stream.avail_in = original.size();
  stream.next_in =
      const_cast<Bytef*>(reinterpret_cast<const Bytef*>(original.data()));

  std::string compressed;
  char chunk[1024];
  do {
    stream.avail_out = sizeof(chunk);
    stream.next_out = reinterpret_cast<Bytef*>(chunk);
    ret = deflate(&stream, Z_FINISH);
    ASSERT_TRUE(ret == Z_OK || ret == Z_STREAM_END);
    compressed.append(chunk, sizeof(chunk) - stream.avail_out);
  } while (ret != Z_STREAM_END);
  deflateEnd(&stream);

  // Verify it is not a gzip stream (it has zlib 0x78 header instead of
  // 0x1f 0x8b).
  EXPECT_FALSE(is_gzip_content(compressed));
  EXPECT_EQ(static_cast<uint8_t>(compressed[0]), 0x78);

  // AUTO_DETECT_WINDOW_BITS should successfully decompress it
  std::string decompressed = gzip_decompress(compressed);
  EXPECT_EQ(decompressed, original);
}

} // namespace
} // namespace howling::net

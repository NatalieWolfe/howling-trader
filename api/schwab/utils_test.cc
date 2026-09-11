#include "api/schwab/utils.h"

#include <string>

#include "boost/beast/core/ostream.hpp"
#include "boost/beast/http/dynamic_body.hpp"
#include "boost/beast/http/field.hpp"
#include "boost/beast/http/message.hpp"
#include "boost/beast/http/status.hpp"
#include "net/gzip.h"
#include "gtest/gtest.h"

namespace howling::schwab {
namespace {

TEST(SchwabUtilsTest, UncompressedBody) {
  boost::beast::http::response<boost::beast::http::dynamic_body> res{
      boost::beast::http::status::ok, 11};
  const std::string original = R"({"status":"ok"})";
  boost::beast::ostream(res.body()) << original;

  EXPECT_EQ(get_response_body(res), original);
}

TEST(SchwabUtilsTest, GzipCompressedBody) {
  boost::beast::http::response<boost::beast::http::dynamic_body> res{
      boost::beast::http::status::ok, 11};
  res.set(boost::beast::http::field::content_encoding, "gzip");
  const std::string original = R"({"status":"ok","data":[1,2,3]})";
  std::string compressed = net::gzip_compress(original);
  boost::beast::ostream(res.body()) << compressed;

  EXPECT_EQ(get_response_body(res), original);
}

TEST(SchwabUtilsTest, TransferEncodingCompressedBody) {
  boost::beast::http::response<boost::beast::http::dynamic_body> res{
      boost::beast::http::status::ok, 11};
  res.set(boost::beast::http::field::transfer_encoding, "gzip");
  const std::string original = R"({"stream":"chunks"})";
  std::string compressed = net::gzip_compress(original);
  boost::beast::ostream(res.body()) << compressed;

  EXPECT_EQ(get_response_body(res), original);
}

} // namespace
} // namespace howling::schwab

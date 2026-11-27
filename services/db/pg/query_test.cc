#include "services/db/pg/query.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "services/db/pg/connection.h"
#include "services/db/pg/types.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling::pg {
namespace {

using ::std::chrono::microseconds;
using ::std::chrono::system_clock;
using ::testing::HasSubstr;
using ::testing::ThrowsMessage;

template <typename T>
concept is_bindable_param =
    requires(param_binder& binder, T value) { binder.bind(value); };

TEST(PgTypesTest, MapsCppTypesToExpectedOids) {
  EXPECT_EQ(pg_type_id<bool>(), BOOL_OID);
  EXPECT_EQ(pg_type_id<bytes>(), BYTEA_OID);
  EXPECT_EQ(pg_type_id<int32_t>(), INT4_OID);
  EXPECT_EQ(pg_type_id<int64_t>(), INT8_OID);
  EXPECT_EQ(pg_type_id<std::string_view>(), TEXT_OID);
  EXPECT_EQ(pg_type_id<std::string>(), TEXT_OID);
  EXPECT_EQ(pg_type_id<double>(), FLOAT8_OID);
  EXPECT_EQ(pg_type_id<system_clock::time_point>(), TIMESTAMP_OID);

  EXPECT_EQ(pg_type_id_for<const std::optional<int64_t>&>(), INT8_OID);
  EXPECT_EQ(pg_type_id_for<std::optional<double>>(), FLOAT8_OID);
}

TEST(PgTypesTest, RoundTripsBinaryScalars) {
  std::string encoded_int32 = encode_int32(-123456789);
  EXPECT_EQ(
      decode_int32(
          encoded_int32.data(),
          static_cast<int>(encoded_int32.size()),
          INT4_OID),
      -123456789);

  std::string encoded_int64 = encode_int64(9876543210123LL);
  EXPECT_EQ(
      decode_int64(
          encoded_int64.data(),
          static_cast<int>(encoded_int64.size()),
          INT8_OID),
      9876543210123LL);

  std::string encoded_double = encode_double(123.456789);
  EXPECT_DOUBLE_EQ(
      decode_double(
          encoded_double.data(),
          static_cast<int>(encoded_double.size()),
          FLOAT8_OID),
      123.456789);

  std::string encoded_true = encode_bool(true);
  std::string encoded_false = encode_bool(false);
  EXPECT_TRUE(decode_bool(
      encoded_true.data(), static_cast<int>(encoded_true.size()), BOOL_OID));
  EXPECT_FALSE(decode_bool(
      encoded_false.data(), static_cast<int>(encoded_false.size()), BOOL_OID));

  system_clock::time_point timestamp{PG_EPOCH + microseconds{1234567890LL}};
  std::string encoded_timestamp = encode_timestamp(timestamp);
  EXPECT_EQ(
      decode_timestamp(
          encoded_timestamp.data(),
          static_cast<int>(encoded_timestamp.size()),
          TIMESTAMP_OID),
      timestamp);
  EXPECT_EQ(
      decode_timestamp(
          encoded_timestamp.data(),
          static_cast<int>(encoded_timestamp.size()),
          TIMESTAMPTZ_OID),
      timestamp);
}

TEST(PgTypesTest, DecodesTextAndByteaOids) {
  std::string payload = "NVDA";
  EXPECT_EQ(
      decode_string_view(payload.data(), 4, TEXT_OID),
      std::string_view{"NVDA"});
  EXPECT_EQ(
      decode_string_view(payload.data(), 4, VARCHAR_OID),
      std::string_view{"NVDA"});
  EXPECT_EQ(
      decode_string_view(payload.data(), 4, NAME_OID),
      std::string_view{"NVDA"});
  EXPECT_EQ(
      decode_string_view(payload.data(), 4, BYTEA_OID),
      std::string_view{"NVDA"});
}

TEST(PgTypesTest, RejectsNumericAndMismatchedOids) {
  std::string eight_bytes(8, '\0');
  std::string four_bytes(4, '\0');
  std::string one_byte(1, '\1');

  // NUMERIC (OID 1700) must fail loudly instead of silently corrupting double.
  EXPECT_THAT(
      [&]() {
        (void)decode_double(eight_bytes.data(), 8, NUMERIC_OID, /*col=*/3);
      },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 3 type mismatch for double")));

  // NUMERIC (OID 1700) must fail loudly when read as int64_t (e.g. uncast SUM).
  EXPECT_THAT(
      [&]() {
        (void)decode_int64(eight_bytes.data(), 8, NUMERIC_OID, /*col=*/5);
      },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 5 type mismatch for int64_t")));

  EXPECT_THAT(
      [&]() { (void)decode_int32(four_bytes.data(), 4, INT8_OID, /*col=*/1); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 1 type mismatch for int32_t")));

  EXPECT_THAT(
      [&]() { (void)decode_bool(one_byte.data(), 1, INT4_OID, /*col=*/2); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 2 type mismatch for bool")));

  EXPECT_THAT(
      [&]() {
        (void)decode_timestamp(eight_bytes.data(), 8, INT8_OID, /*col=*/0);
      },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 0 type mismatch for time_point")));

  EXPECT_THAT(
      [&]() {
        (void)decode_string_view(eight_bytes.data(), 8, NUMERIC_OID, /*col=*/4);
      },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 4 type mismatch for string_view")));
}

TEST(PgTypesTest, RejectsMismatchedBinaryLengths) {
  std::string eight_bytes(8, '\0');
  std::string four_bytes(4, '\0');
  std::string two_bytes(2, '\0');

  EXPECT_THAT(
      [&]() {
        (void)decode_double(four_bytes.data(), 4, FLOAT8_OID, /*col=*/1);
      },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 1 length mismatch for double")));

  EXPECT_THAT(
      [&]() { (void)decode_int32(eight_bytes.data(), 8, INT4_OID, /*col=*/2); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 2 length mismatch for int32_t")));

  EXPECT_THAT(
      [&]() { (void)decode_int64(four_bytes.data(), 4, INT8_OID, /*col=*/3); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 3 length mismatch for int64_t")));

  EXPECT_THAT(
      [&]() { (void)decode_bool(two_bytes.data(), 2, BOOL_OID, /*col=*/0); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 0 length mismatch for bool")));

  EXPECT_THAT(
      [&]() {
        (void)decode_timestamp(four_bytes.data(), 4, TIMESTAMP_OID, /*col=*/4);
      },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 4 length mismatch for time_point")));
}

TEST(PgTypesTest, RejectsNegativeLengthAndNullDataPointers) {
  std::string payload = "hello";
  constexpr std::array<unsigned int, 1> TEXT_OIDS{TEXT_OID};

  EXPECT_THAT(
      [&]() {
        validate_column_type(
            /*col=*/2,
            TEXT_OID,
            /*actual_length=*/-1,
            TEXT_OIDS,
            std::nullopt,
            "string_view");
      },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 2 has negative length -1 for string_view")));

  EXPECT_THAT(
      [&]() {
        (void)decode_string_view(payload.data(), -5, TEXT_OID, /*col=*/1);
      },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 1 has negative length -5 for string_view")));

  EXPECT_THAT(
      [&]() { (void)decode_int32(nullptr, 4, INT4_OID, /*col=*/0); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 0 has null data pointer for int32_t")));

  EXPECT_THAT(
      [&]() { (void)decode_int64(nullptr, 8, INT8_OID, /*col=*/1); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 1 has null data pointer for int64_t")));

  EXPECT_THAT(
      [&]() { (void)decode_double(nullptr, 8, FLOAT8_OID, /*col=*/2); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 2 has null data pointer for double")));

  EXPECT_THAT(
      [&]() { (void)decode_bool(nullptr, 1, BOOL_OID, /*col=*/3); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 3 has null data pointer for bool")));

  EXPECT_THAT(
      [&]() { (void)decode_timestamp(nullptr, 8, TIMESTAMP_OID, /*col=*/4); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 4 has null data pointer for time_point")));

  EXPECT_THAT(
      [&]() { (void)decode_string_view(nullptr, 0, TEXT_OID, /*col=*/5); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("column 5 has null data pointer for string_view")));
}

TEST(ParamBinderTest, BindsBinaryParamsAndNullOptionals) {
  param_binder binder;
  binder.reserve(5);
  binder.bind(static_cast<int32_t>(42));
  binder.bind(std::optional<int64_t>{std::nullopt});
  binder.bind(std::optional<double>{3.5});
  binder.bind(std::string_view{"AAPL"});
  binder.bind(true);

  ASSERT_EQ(binder.count(), 5);
  EXPECT_EQ(binder.types()[0], INT4_OID);
  EXPECT_EQ(binder.lengths()[0], 4);
  EXPECT_EQ(binder.formats()[0], BINARY_FORMAT);
  EXPECT_EQ(
      decode_int32(binder.values()[0], binder.lengths()[0], INT4_OID), 42);

  EXPECT_EQ(binder.types()[1], INT8_OID);
  EXPECT_EQ(binder.values()[1], nullptr);
  EXPECT_EQ(binder.lengths()[1], 0);
  EXPECT_EQ(binder.formats()[1], BINARY_FORMAT);

  EXPECT_EQ(binder.types()[2], FLOAT8_OID);
  EXPECT_EQ(binder.lengths()[2], 8);
  EXPECT_DOUBLE_EQ(
      decode_double(binder.values()[2], binder.lengths()[2], FLOAT8_OID), 3.5);

  EXPECT_EQ(binder.types()[3], TEXT_OID);
  EXPECT_EQ(binder.lengths()[3], 4);
  EXPECT_EQ(
      decode_string_view(binder.values()[3], binder.lengths()[3], TEXT_OID),
      "AAPL");

  EXPECT_EQ(binder.types()[4], BOOL_OID);
  EXPECT_EQ(binder.lengths()[4], 1);
  EXPECT_TRUE(decode_bool(binder.values()[4], binder.lengths()[4], BOOL_OID));

  binder.clear();
  EXPECT_EQ(binder.count(), 0);
  EXPECT_EQ(binder.types(), nullptr);
  EXPECT_EQ(binder.values(), nullptr);
}

TEST(ParamBinderTest, BindsStringBytesTimestampAndCStringAsText) {
  static_assert(!is_bindable_param<int*>);
  static_assert(!is_bindable_param<const void*>);
  static_assert(is_bindable_param<const char*>);
  static_assert(is_bindable_param<bool>);

  param_binder binder;
  std::string owned_text = "MSFT";
  bytes raw_payload{std::string{"\x01\x00\x02", 3}};
  system_clock::time_point timestamp{PG_EPOCH + microseconds{987654321LL}};

  binder.bind(owned_text);
  binder.bind(raw_payload);
  binder.bind(timestamp);
  // Regression test: string literals must bind as TEXT, not decay to bool.
  binder.bind("NVDA");

  ASSERT_EQ(binder.count(), 4);

  EXPECT_EQ(binder.types()[0], TEXT_OID);
  EXPECT_EQ(binder.lengths()[0], 4);
  EXPECT_EQ(
      decode_string_view(binder.values()[0], binder.lengths()[0], TEXT_OID),
      "MSFT");

  EXPECT_EQ(binder.types()[1], BYTEA_OID);
  EXPECT_EQ(binder.lengths()[1], 3);
  EXPECT_EQ(
      decode_string_view(binder.values()[1], binder.lengths()[1], BYTEA_OID),
      std::string_view("\x01\x00\x02", 3));

  EXPECT_EQ(binder.types()[2], TIMESTAMP_OID);
  EXPECT_EQ(binder.lengths()[2], 8);
  EXPECT_EQ(
      decode_timestamp(binder.values()[2], binder.lengths()[2], TIMESTAMP_OID),
      timestamp);

  EXPECT_EQ(binder.types()[3], TEXT_OID);
  EXPECT_EQ(binder.lengths()[3], 4);
  EXPECT_EQ(
      decode_string_view(binder.values()[3], binder.lengths()[3], TEXT_OID),
      "NVDA");

  const char* null_string = nullptr;
  EXPECT_THROW(binder.bind(null_string), std::invalid_argument);
}

TEST(PgConnectionTest, BuildsConninfoString) {
  postgres_options options{
      .host = "db.internal",
      .port = "5432",
      .user = "reader",
      .password = "secret",
      .dbname = "howling",
      .sslmode = "require"};
  EXPECT_EQ(
      build_conninfo(options),
      "host='db.internal' port='5432' dbname='howling' user='reader' "
      "password='secret' sslmode='require'");
}

TEST(PgConnectionTest, EscapesSpecialCharactersInConninfo) {
  postgres_options options{
      .host = "db host.local",
      .port = "5432",
      .user = "user'name",
      .password = R"(p@ss'w\ord with spaces)",
      .dbname = R"(howl\ing'db)",
      .sslmode = "verify-full"};
  EXPECT_EQ(
      build_conninfo(options),
      R"(host='db host.local' port='5432' dbname='howl\\ing\'db' )"
      R"(user='user\'name' password='p@ss\'w\\ord with spaces' )"
      R"(sslmode='verify-full')");
}

} // namespace
} // namespace howling::pg

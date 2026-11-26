#include "data/wilder_rma.h"

#include <stdexcept>

#include "gtest/gtest.h"

namespace howling {
namespace {

TEST(WilderRmaTest, RejectsNonPositivePeriod) {
  EXPECT_THROW(wilder_rma(0), std::invalid_argument);
  EXPECT_THROW(wilder_rma(-5), std::invalid_argument);
}

TEST(WilderRmaTest, SeedsWithSmaAndSmoothsSubsequentSamples) {
  wilder_rma rma(3);
  EXPECT_EQ(rma.period(), 3);
  EXPECT_FALSE(rma.has_value());
  EXPECT_FALSE(rma.value().has_value());

  EXPECT_FALSE(rma.update(10.0));
  EXPECT_FALSE(rma.has_value());
  EXPECT_FALSE(rma.update(20.0));
  EXPECT_FALSE(rma.has_value());
  EXPECT_TRUE(rma.update(30.0));
  ASSERT_TRUE(rma.has_value());
  ASSERT_TRUE(rma.value().has_value());
  // Seed SMA is (10 + 20 + 30) / 3 = 20.
  EXPECT_DOUBLE_EQ(*rma.value(), 20.0);

  // Next RMA is ((20 * 2) + 50) / 3 = 30.
  EXPECT_TRUE(rma.update(50.0));
  EXPECT_DOUBLE_EQ(*rma.value(), 30.0);

  // Next RMA is ((30 * 2) + 15) / 3 = 25.
  EXPECT_TRUE(rma.update(15.0));
  EXPECT_DOUBLE_EQ(*rma.value(), 25.0);

  // Reset clears state and restarts seeding.
  rma.reset();
  EXPECT_FALSE(rma.has_value());
  EXPECT_FALSE(rma.value().has_value());
  EXPECT_FALSE(rma.update(3.0));
  EXPECT_FALSE(rma.update(6.0));
  EXPECT_TRUE(rma.update(9.0));
  ASSERT_TRUE(rma.has_value());
  EXPECT_DOUBLE_EQ(*rma.value(), 6.0);
}

TEST(WilderRmaTest, PeriodOneTracksLatestSample) {
  wilder_rma rma(1);
  EXPECT_TRUE(rma.update(42.5));
  ASSERT_TRUE(rma.has_value());
  EXPECT_DOUBLE_EQ(*rma.value(), 42.5);

  EXPECT_TRUE(rma.update(17.25));
  EXPECT_DOUBLE_EQ(*rma.value(), 17.25);
}

} // namespace
} // namespace howling

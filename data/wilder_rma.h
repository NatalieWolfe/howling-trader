#pragma once

#include <optional>

namespace howling {

/**
 * @brief Wilder's Running Moving Average (RMA), seeded with a simple moving
 * average over the first period samples.
 *
 * Thread-compatible: external synchronization is required for concurrent
 * access.
 */
class wilder_rma {
public:
  /**
   * @brief Constructs a Wilder RMA tracker with the given smoothing period.
   *
   * @param period Positive number of samples for seeding and smoothing.
   * @throws std::invalid_argument if period <= 0.
   */
  explicit wilder_rma(int period);

  /**
   * @brief Incorporates the next sample into the running moving average.
   *
   * @param sample New input value.
   * @return True if the smoothed value is available after this update.
   */
  bool update(double sample);

  /**
   * @brief Returns true once at least period() samples have been observed.
   */
  [[nodiscard]] bool has_value() const { return _value.has_value(); }

  /**
   * @brief Returns the current smoothed value, or std::nullopt if not ready.
   */
  [[nodiscard]] std::optional<double> value() const { return _value; }

  /**
   * @brief Returns the configured smoothing period.
   */
  [[nodiscard]] int period() const { return _period; }

  /**
   * @brief Clears all accumulated state so seeding starts over.
   */
  void reset();

private:
  int _period;
  int _sample_count = 0;
  double _seed_sum = 0.0;
  std::optional<double> _value;
};

} // namespace howling

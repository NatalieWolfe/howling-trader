#include "data/wilder_rma.h"

#include <stdexcept>

namespace howling {

wilder_rma::wilder_rma(int period) : _period(period) {
  if (_period <= 0) {
    throw std::invalid_argument("Wilder RMA period must be positive.");
  }
}

bool wilder_rma::update(double sample) {
  if (++_sample_count <= _period) {
    _seed_sum += sample;
    if (_sample_count == _period) {
      _value = _seed_sum / static_cast<double>(_period);
    }
    return has_value();
  }

  *_value = ((*_value * (_period - 1)) + sample) / static_cast<double>(_period);
  return true;
}

void wilder_rma::reset() {
  _sample_count = 0;
  _seed_sum = 0.0;
  _value.reset();
}

} // namespace howling

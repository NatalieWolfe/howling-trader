#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace howling {

template <typename T>
class vector : public std::vector<T> {
public:
  using base = std::vector<T>;
  using base::size;
  using std::vector<T>::vector;

  vector(base&& other) : base(std::move(other)) {}
  vector(const base& other) : base(other) {}
  vector& operator=(base&& other) {
    base::operator=(std::move(other));
    return *this;
  }
  vector& operator=(const base& other) {
    base::operator=(other);
    return *this;
  }

  T& operator()(int64_t i) { return (*this)[_normalize(i)]; }
  const T& operator()(int64_t i) const { return base::at(_normalize(i)); }

  std::span<T> operator()(int64_t start, int64_t end) {
    start = _normalize(start);
    end = _normalize(end);
    return std::span<T>{this->begin() + start, this->begin() + end};
  }

  std::span<const T> operator()(int64_t start, int64_t end) const {
    start = _normalize(start);
    end = _normalize(end);
    return std::span<const T>{this->begin() + start, this->begin() + end};
  }

  // Returns the subspan of `n` elements which contains the last index.
  std::span<T> last_n(int64_t n) { return (*this)(-n, base::size()); }
  std::span<const T> last_n(int64_t n) const {
    return (*this)(-n, base::size());
  }

  // Returns the subspan of `n` elements just before the last index.
  std::span<T> previous_n(int64_t n) {
    return (*this)(-n - 1, base::size() - 1);
  }
  std::span<const T> previous_n(int64_t n) const {
    return (*this)(-n - 1, base::size() - 1);
  }

private:
  std::size_t _normalize(int64_t i) const {
    if (i >= 0) return i;
    if (static_cast<size_t>(-i) >= base::size()) return 0;
    return base::size() + i;
  }
};

} // namespace howling

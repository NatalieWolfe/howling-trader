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
  using base::vector;

  vector(base&& other) : base(std::move(other)) {}
  vector(const base& other) : base(other) {}
  vector& operator=(base&& other) {
    static_cast<base&>(*this) = std::move(other);
    return *this;
  }
  vector& operator=(const base& other) {
    static_cast<base&>(*this) = other;
    return *this;
  }

  T& operator()(int64_t i) { return (*this)[i]; }
  const T& operator()(int64_t i) const { return at(_normalize(i)); }

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

  std::span<T> last_n(int64_t n) { return (*this)(-n, this->size()); }
  std::span<const T> last_n(int64_t n) const {
    return (*this)(-n, this->size());
  }

private:
  std::size_t _normalize(int64_t i) const {
    if (i >= 0) return i;
    return this->size() + i;
  }
};

} // namespace howling

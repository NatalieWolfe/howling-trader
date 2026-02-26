#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace howling {

/**
 * @brief A thread-safe circular buffer.
 *
 * This class is internally synchronized and safe for concurrent use by
 * multiple readers and a single writer.
 */
template <typename T>
class circular_buffer {
private:
  static const size_t END = std::numeric_limits<size_t>::max();

public:
  explicit circular_buffer(uint32_t capacity) : _capacity{capacity} {
    _buffer.reserve(capacity);
  }

  ~circular_buffer() = default;
  circular_buffer(const circular_buffer&) = default;
  circular_buffer(circular_buffer&&) = default;
  circular_buffer& operator=(const circular_buffer&) = default;
  circular_buffer& operator=(circular_buffer&&) = default;

  class iterator;
  class const_iterator;

  [[nodiscard]] iterator begin() {
    std::lock_guard lock{_mutex};
    return iterator{_front_index_locked(), *this};
  }
  [[nodiscard]] const_iterator begin() const {
    std::lock_guard lock{_mutex};
    return const_iterator{_front_index_locked(), *this};
  }
  [[nodiscard]] const_iterator cbegin() const { return begin(); }
  [[nodiscard]] iterator end() { return iterator{END, *this}; }
  [[nodiscard]] const_iterator end() const {
    return const_iterator{END, *this};
  }
  [[nodiscard]] const_iterator cend() const { return end(); }

  [[nodiscard]] T at(uint32_t index) const {
    std::lock_guard lock{_mutex};
    if (index >= _size) {
      throw std::range_error("Out of bounds offset into circular buffer.");
    }
    return _buffer[_circularize(_front_index_locked() + index)];
  }

  [[nodiscard]] T operator[](uint32_t index) const {
    std::lock_guard lock{_mutex};
    return _buffer[_circularize(_front_index_locked() + index)];
  }

  template <typename U>
  void push_back(U&& val) {
    std::lock_guard lock{_mutex};
    if (_buffer.size() < _capacity) {
      _buffer.push_back(std::forward<U>(val));
    } else {
      _buffer[_circularize(_insert_count)] = std::forward<U>(val);
    }
    ++_insert_count;
    if (_size < _capacity) ++_size;
  }

  void pop_front() {
    std::lock_guard lock{_mutex};
    if (_size == 0) {
      throw std::range_error(
          "Circular buffer is empty, cannot pop front element.");
    }
    --_size;
  }

  [[nodiscard]] T front() const {
    std::lock_guard lock{_mutex};
    if (_size == 0) throw std::range_error("Circular buffer is empty.");
    return _buffer[_circularize(_front_index_locked())];
  }
  [[nodiscard]] T back() const {
    std::lock_guard lock{_mutex};
    if (_size == 0) throw std::range_error("Circular buffer is empty.");
    return _buffer[_circularize(_back_index_locked())];
  }

  [[nodiscard]] size_t capacity() const { return _capacity; }
  [[nodiscard]] size_t size() const {
    std::lock_guard lock{_mutex};
    return _size;
  }
  [[nodiscard]] bool empty() const {
    std::lock_guard lock{_mutex};
    return _size == 0;
  }
  void clear() {
    std::lock_guard lock{_mutex};
    _buffer.clear();
    _size = 0;
    _insert_count = 0;
  }

private:
  friend class iterator;
  friend class const_iterator;

  size_t _front_index_locked() const {
    return _insert_count > _size ? _insert_count - _size : 0;
  }
  size_t _back_index_locked() const { return _insert_count - 1; }
  size_t _circularize(size_t index) const { return index % _capacity; }

  size_t _capacity;
  size_t _size = 0;
  size_t _insert_count = 0;
  std::vector<T> _buffer;
  mutable std::mutex _mutex;
};

template <typename T>
class circular_buffer<T>::iterator {
public:
  ~iterator() = default;
  iterator(const iterator&) = default;
  iterator(iterator&&) = default;
  iterator& operator=(const iterator&) = default;
  iterator& operator=(iterator&&) = default;

  iterator& operator++() {
    std::lock_guard lock{_buffer->_mutex};
    ++_index;
    _jump_to_front_locked();
    return *this;
  }
  iterator operator++(int) {
    iterator val = *this;
    ++(*this);
    return val;
  }

  iterator& operator+=(uint32_t n) {
    std::lock_guard lock{_buffer->_mutex};
    _index += n;
    _jump_to_front_locked();
    return *this;
  }

  friend iterator operator+(const iterator& itr, uint32_t n) {
    iterator val = itr;
    val += n;
    return val;
  }

  bool operator==(const iterator& other) const {
    std::lock_guard lock{_buffer->_mutex};
    if (_is_end_locked() || other._is_end_locked()) {
      return _is_end_locked() == other._is_end_locked();
    }
    return _index == other._index && _buffer == other._buffer;
  }
  bool operator!=(const iterator& other) const { return !(*this == other); }

  T operator*() const {
    std::lock_guard lock{_buffer->_mutex};
    _jump_to_front_locked();
    return _buffer->_buffer[_buffer->_circularize(_index)];
  }

private:
  friend class circular_buffer<T>;
  friend class circular_buffer<T>::const_iterator;

  iterator(size_t index, circular_buffer& buffer)
      : _index{index}, _buffer{&buffer} {}

  void _jump_to_front_locked() const {
    size_t front = _buffer->_front_index_locked();
    if (_index < front) _index = front;
  }

  bool _is_end_locked() const {
    return _index == circular_buffer::END || _index >= _buffer->_insert_count;
  }

  mutable size_t _index;
  circular_buffer* _buffer;
};

template <typename T>
class circular_buffer<T>::const_iterator {
public:
  ~const_iterator() = default;
  const_iterator(const const_iterator&) = default;
  const_iterator(const_iterator&&) = default;
  const_iterator(const iterator& other)
      : _index{other._index}, _buffer{other._buffer} {}
  const_iterator& operator=(const const_iterator&) = default;
  const_iterator& operator=(const_iterator&&) = default;

  const_iterator& operator++() {
    std::lock_guard lock{_buffer->_mutex};
    ++_index;
    _jump_to_front_locked();
    return *this;
  }
  const_iterator operator++(int) {
    const_iterator val = *this;
    ++(*this);
    return val;
  }

  bool operator==(const const_iterator& other) const {
    std::lock_guard lock{_buffer->_mutex};
    if (_is_end_locked() || other._is_end_locked()) {
      return _is_end_locked() == other._is_end_locked();
    }
    return _index == other._index && _buffer == other._buffer;
  }
  bool operator!=(const const_iterator& other) const {
    return !(*this == other);
  }

  T operator*() const {
    std::lock_guard lock{_buffer->_mutex};
    _jump_to_front_locked();
    return _buffer->_buffer[_buffer->_circularize(_index)];
  }

private:
  friend class circular_buffer<T>;
  friend class circular_buffer<T>::iterator;

  const_iterator(size_t index, const circular_buffer& buffer)
      : _index{index}, _buffer{&buffer} {}

  void _jump_to_front_locked() const {
    size_t front = _buffer->_front_index_locked();
    if (_index < front) _index = front;
  }

  bool _is_end_locked() const {
    return _index == circular_buffer::END || _index >= _buffer->_insert_count;
  }

  mutable size_t _index;
  const circular_buffer* _buffer;
};

} // namespace howling

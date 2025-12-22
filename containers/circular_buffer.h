#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace howling {

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

  iterator begin() { return iterator{_front_index(), *this}; }
  const_iterator begin() const { return const_iterator{_front_index(), *this}; }
  const_iterator cbegin() const { return begin(); }
  iterator end() { return iterator{END, *this}; }
  const_iterator end() const { return const_iterator{END, *this}; }
  const_iterator cend() const { return end(); }

  T& at(uint32_t index) {
    if (index >= _size) {
      throw std::range_error("Out of bounds offset into circular buffer.");
    }
    return _buffer.at(_circularize(_front_index() + index));
  }
  const T& at(uint32_t index) const {
    if (index >= _size) {
      throw std::range_error("Out of bounds offset into circular buffer.");
    }
    return _buffer.at(_circularize(_front_index() + index));
  }

  T& operator[](uint32_t index) {
    return _buffer[_circularize(_front_index() + index)];
  }

  template <typename U>
  void push_back(U&& val) {
    if (_buffer.size() < _capacity) {
      _buffer.push_back(std::forward<U>(val));
    } else {
      _buffer[_circularize(_insert_count)] = std::forward<U>(val);
    }
    ++_insert_count;
    if (_size < _capacity) ++_size;
  }

  void pop_front() {
    if (_size == 0) {
      throw std::range_error(
          "Circular buffer is empty, cannot pop front element.");
    }
    --_size;
  }

  T& front() { return _buffer.at(_circularize(_front_index())); }
  T& back() { return _buffer.at(_circularize(_back_index())); }
  const T& front() const { return _buffer.at(_circularize(_front_index())); }
  const T& back() const { return _buffer.at(_circularize(_back_index())); }

  size_t capacity() const { return _capacity; }
  size_t size() const { return _size; }
  bool empty() const { return _size == 0; }
  void clear() {
    _buffer.clear();
    _size = 0;
    _insert_count = 0;
  }

private:
  size_t _front_index() const { return _insert_count - _size; }
  size_t _back_index() const { return _insert_count - 1; }
  size_t _circularize(size_t index) const { return index % _capacity; }

  size_t _capacity;
  std::atomic_size_t _size = 0;
  std::atomic_size_t _insert_count = 0;
  std::vector<T> _buffer;
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
    ++_index;
    _jump_to_front();
    return *this;
  }
  iterator operator++(int) {
    iterator val = *this;
    ++(*this);
    return val;
  }

  iterator& operator+=(uint32_t n) {
    _index += n;
    _jump_to_front();
    return *this;
  }

  friend iterator operator+(const iterator& itr, uint32_t n) {
    iterator val = itr;
    val += n;
    return val;
  }
  friend iterator operator+(uint32_t n, const iterator& itr) {
    iterator val = itr;
    val += n;
    return val;
  }

  bool operator==(const iterator& other) const {
    if (_is_end() || other._is_end()) return _is_end() == other._is_end();
    return _index == other._index && _buffer == other._buffer;
  }
  bool operator==(const const_iterator& other) const {
    if (_is_end() || other._is_end()) return _is_end() == other._is_end();
    return _index == other._index && _buffer == other._buffer;
  }

  T& operator*() const {
    return _buffer->_buffer[_buffer->_circularize(_index)];
  }
  T* operator->() const {
    return &_buffer->_buffer[_buffer->_circularize(_index)];
  }

private:
  friend class circular_buffer<T>;
  friend class circular_buffer<T>::const_iterator;

  iterator(size_t index, circular_buffer& buffer)
      : _index{index}, _buffer{&buffer} {}

  void _jump_to_front() {
    if (_buffer->_insert_count - _index > _buffer->_size) {
      _index = _buffer->_front_index();
    }
  }

  bool _is_end() const {
    return _index == circular_buffer::END || _index == _buffer->_insert_count;
  }

  size_t _index;
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
  const_iterator& operator=(const iterator& other) {
    _index = other._index;
    _buffer = other._buffer;
    return *this;
  }

  const_iterator& operator++() {
    ++_index;
    _jump_to_front();
    return *this;
  }
  const_iterator operator++(int) {
    const_iterator val = *this;
    ++(*this);
    return val;
  }

  const_iterator& operator+=(uint32_t n) {
    _index += n;
    _jump_to_front();
    return *this;
  }

  friend const_iterator operator+(const const_iterator& itr, uint32_t n) {
    const_iterator val = itr;
    val += n;
    return val;
  }
  friend const_iterator operator+(uint32_t n, const const_iterator& itr) {
    const_iterator val = itr;
    val += n;
    return val;
  }

  bool operator==(const const_iterator& other) const {
    if (_is_end() || other._is_end()) return _is_end() == other._is_end();
    return _index == other._index && _buffer == other._buffer;
  }
  bool operator==(const iterator& other) const {
    if (_is_end() || other._is_end()) return _is_end() == other._is_end();
    return _index == other._index && _buffer == other._buffer;
  }

  const T& operator*() const {
    return _buffer->_buffer.at(_buffer->_circularize(_index));
  }
  const T* operator->() const {
    return &_buffer->_buffer.at(_buffer->_circularize(_index));
  }

private:
  friend class circular_buffer<T>;
  friend class circular_buffer<T>::iterator;

  const_iterator(size_t index, const circular_buffer& buffer)
      : _index{index}, _buffer{&buffer} {}

  void _jump_to_front() {
    if (_buffer->_insert_count - _index > _buffer->_size) {
      _index = _buffer->_front_index();
    }
  }

  bool _is_end() const {
    return _index == circular_buffer::END || _index == _buffer->_insert_count;
  }

  size_t _index;
  const circular_buffer* _buffer;
};

} // namespace howling

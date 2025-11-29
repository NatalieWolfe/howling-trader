#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace howling {

template <typename T>
class circular_buffer {
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

  iterator begin() { return iterator{0, *this}; }
  const_iterator begin() const { return const_iterator{0, *this}; }
  const_iterator cbegin() const { return begin(); }
  iterator end() { return iterator{_size, *this}; }
  const_iterator end() const { return const_iterator{_size, *this}; }
  const_iterator cend() const { return end(); }

  T& at(uint32_t index) {
    if (index >= _size) {
      throw std::range_error("Out of bounds offset into circular buffer.");
    }
    return _buffer.at(_circularize(_front + index));
  }
  const T& at(uint32_t index) const {
    if (index >= _size) {
      throw std::range_error("Out of bounds offset into circular buffer.");
    }
    return _buffer.at(_circularize(_front + index));
  }

  T& operator[](uint32_t index) {
    return _buffer[_circularize(_front + index)];
  }

  template <typename U>
  void push_back(U&& val) {
    if (_buffer.size() < _capacity) {
      _buffer.push_back(std::forward<U>(val));
      ++_size;
      _increment(_back);
      return;
    }

    _buffer[_back] = std::forward<U>(val);
    _increment(_back);
    if (_size == _capacity) {
      _increment(_front);
    } else {
      ++_size;
    }
  }

  void pop_front() {
    if (_size == 0) {
      throw std::range_error(
          "Circular buffer is empty, cannot pop front element.");
    }
    _increment(_front);
    --_size;
  }

  T& front() { return _buffer.at(_front); }
  T& back() { return _buffer.at(_circularize((_back + _capacity) - 1)); }
  const T& front() const { return _buffer.at(_front); }
  const T& back() const {
    return _buffer.at(_circularize((_back + _capacity) - 1));
  }

  size_t capacity() const { return _capacity; }
  size_t size() const { return _size; }
  bool empty() const { return _size == 0; }
  void clear() {
    _buffer.clear();
    _size = 0;
    _front = 0;
    _back = 0;
  }

private:
  size_t _circularize(size_t index) const { return index % _capacity; }
  void _increment(size_t& index) const { index = _circularize(index + 1); }

  size_t _capacity;
  size_t _front = 0;
  size_t _back = 0;
  size_t _size = 0;
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
    return *this;
  }
  iterator operator++(int) {
    iterator val = *this;
    ++(*this);
    return val;
  }

  iterator& operator+=(uint32_t n) {
    _index += n;
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
    return _index == other._index && _buffer == other._buffer;
  }
  bool operator==(const const_iterator& other) const {
    return _index == other._index && _buffer == other._buffer;
  }

  T& operator*() const { return (*_buffer)[_index % _buffer->_size]; }
  T* operator->() const { return &(*_buffer)[_index % _buffer->_size]; }

private:
  friend class circular_buffer<T>;
  friend class circular_buffer<T>::const_iterator;

  iterator(size_t index, circular_buffer& buffer)
      : _index{index}, _buffer{&buffer} {}

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
    return *this;
  }
  const_iterator operator++(int) {
    const_iterator val = *this;
    ++(*this);
    return val;
  }

  const_iterator& operator+=(uint32_t n) {
    _index += n;
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
    return _index == other._index && _buffer == other._buffer;
  }
  bool operator==(const iterator& other) const {
    return _index == other._index && _buffer == other._buffer;
  }

  const T& operator*() const { return _buffer->at(_index % _buffer->_size); }
  const T* operator->() const { return &_buffer->at(_index % _buffer->_size); }

private:
  friend class circular_buffer<T>;
  friend class circular_buffer<T>::iterator;

  const_iterator(size_t index, const circular_buffer& buffer)
      : _index{index}, _buffer{&buffer} {}

  size_t _index;
  const circular_buffer* _buffer;
};

} // namespace howling

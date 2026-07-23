#pragma once

#include <cstddef>
#include <iterator>

namespace util {

template <class I>
struct Range {
  I begin_;
  I end_;

  Range() : begin_(), end_() {}
  Range(I b, I e) : begin_(b), end_(e) {}

  I begin() const { return begin_; }
  I end() const { return end_; }
  bool empty() const { return begin_ == end_; }
  std::size_t size() const { return end_ - begin_; }

  typename std::iterator_traits<I>::reference front() const { return *begin_; }
  typename std::iterator_traits<I>::reference back() const { return *(end_ - 1); }

  void advance_begin(std::size_t n) { begin_ += n; }
};

} // namespace util

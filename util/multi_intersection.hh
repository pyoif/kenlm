#ifndef UTIL_MULTI_INTERSECTION_H
#define UTIL_MULTI_INTERSECTION_H

#include "range.hh"

#include <cassert>
#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

namespace util {

namespace detail {
template <class Range> struct RangeLessBySize {
  bool operator()(const Range &left, const Range &right) const {
    return left.size() < right.size();
  }
};

/* Takes sets specified by their iterators and a std::optional containing
 * the lowest intersection if any.  Each set must be sorted in increasing
 * order.  sets is changed to truncate the beginning of each sequence to the
 * location of the match or an empty set.  Precondition: sets is not empty
 * since the intersection over null is the universe and this function does not
 * know the universe.
 */
template <class Iterator, class Less> std::optional<typename std::iterator_traits<Iterator>::value_type> FirstIntersectionSorted(std::vector<util::Range<Iterator> > &sets, const Less &less = std::less<typename std::iterator_traits<Iterator>::value_type>()) {
  typedef std::vector<util::Range<Iterator> > Sets;
  typedef typename std::iterator_traits<Iterator>::value_type Value;

  assert(!sets.empty());

  if (sets.front().empty()) return std::nullopt;
  // Possibly suboptimal to copy for general Value; makes unsigned int go slightly faster.
  Value highest(sets.front().front());
  for (typename Sets::iterator i(sets.begin()); i != sets.end(); ) {
    i->advance_begin(std::lower_bound(i->begin(), i->end(), highest, less) - i->begin());
    if (i->empty()) return std::nullopt;
    if (less(highest, i->front())) {
      highest = i->front();
      // start over
      i = sets.begin();
    } else {
      ++i;
    }
  }
  return highest;
}

} // namespace detail

template <class Iterator, class Less> std::optional<typename std::iterator_traits<Iterator>::value_type> FirstIntersection(std::vector<util::Range<Iterator> > &sets, const Less less) {
  assert(!sets.empty());

  std::sort(sets.begin(), sets.end(), detail::RangeLessBySize<util::Range<Iterator> >());
  return detail::FirstIntersectionSorted(sets, less);
}

template <class Iterator> std::optional<typename std::iterator_traits<Iterator>::value_type> FirstIntersection(std::vector<util::Range<Iterator> > &sets) {
  return FirstIntersection(sets, std::less<typename std::iterator_traits<Iterator>::value_type>());
}

template <class Iterator, class Output, class Less> void AllIntersection(std::vector<util::Range<Iterator> > &sets, Output &out, const Less less) {
  typedef typename std::iterator_traits<Iterator>::value_type Value;
  assert(!sets.empty());

  std::sort(sets.begin(), sets.end(), detail::RangeLessBySize<util::Range<Iterator> >());
  for (std::optional<Value> ret; (ret = detail::FirstIntersectionSorted(sets, less)); sets.front().advance_begin(1)) {
    out(*ret);
  }
}

template <class Iterator, class Output> void AllIntersection(std::vector<util::Range<Iterator> > &sets, Output &out) {
  AllIntersection(sets, out, std::less<typename std::iterator_traits<Iterator>::value_type>());
}

} // namespace util

#endif // UTIL_MULTI_INTERSECTION_H

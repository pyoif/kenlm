#include "multi_intersection.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

namespace util {
namespace {

TEST_CASE("Empty") {
  std::vector<util::Range<const unsigned int*> > sets;

  sets.push_back(util::Range<const unsigned int*>(static_cast<const unsigned int*>(NULL), static_cast<const unsigned int*>(NULL)));
  CHECK_FALSE(FirstIntersection(sets));
}

TEST_CASE("Single") {
  std::vector<unsigned int> nums;
  nums.push_back(1);
  nums.push_back(4);
  nums.push_back(100);
  std::vector<util::Range<std::vector<unsigned int>::const_iterator> > sets;
  sets.push_back(util::Range<std::vector<unsigned int>::const_iterator>(nums.begin(), nums.end()));

  std::optional<unsigned int> ret(FirstIntersection(sets));

  REQUIRE(ret);
  CHECK_EQ(static_cast<unsigned int>(1), *ret);
}

template <class T, unsigned int len> util::Range<const T*> RangeFromArray(const T (&arr)[len]) {
  return util::Range<const T*>(arr, arr + len);
}

TEST_CASE("MultiNone") {
  unsigned int nums0[] = {1, 3, 4, 22};
  unsigned int nums1[] = {2, 5, 12};
  unsigned int nums2[] = {4, 17};

  std::vector<util::Range<const unsigned int*> > sets;
  sets.push_back(RangeFromArray(nums0));
  sets.push_back(RangeFromArray(nums1));
  sets.push_back(RangeFromArray(nums2));

  CHECK_FALSE(FirstIntersection(sets));
}

TEST_CASE("MultiOne") {
  unsigned int nums0[] = {1, 3, 4, 17, 22};
  unsigned int nums1[] = {2, 5, 12, 17};
  unsigned int nums2[] = {4, 17};

  std::vector<util::Range<const unsigned int*> > sets;
  sets.push_back(RangeFromArray(nums0));
  sets.push_back(RangeFromArray(nums1));
  sets.push_back(RangeFromArray(nums2));

  std::optional<unsigned int> ret(FirstIntersection(sets));
  REQUIRE(ret);
  CHECK_EQ(static_cast<unsigned int>(17), *ret);
}

} // namespace
} // namespace util

#include "sized_iterator.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

namespace util { namespace {

struct CompareChar {
  bool operator()(const void *first, const void *second) const {
    return *static_cast<const char*>(first) < *static_cast<const char*>(second);
  }
};

TEST_CASE("sort") {
  char items[3] = {1, 2, 0};
  SizedSort(items, items + 3, 1, CompareChar());
  CHECK_EQ(0, items[0]);
  CHECK_EQ(1, items[1]);
  CHECK_EQ(2, items[2]);
}

}} // namespace anonymous util

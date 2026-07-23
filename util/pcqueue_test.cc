#include "pcqueue.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

namespace util {
namespace {

TEST_CASE("SingleThread") {
  PCQueue<int> queue(10);
  for (int i = 0; i < 10; ++i) {
    queue.Produce(i);
  }
  for (int i = 0; i < 10; ++i) {
    CHECK_EQ(i, queue.Consume());
  }
}

}
} // namespace util

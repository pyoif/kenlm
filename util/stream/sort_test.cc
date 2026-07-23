#include "sort.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <random>

#include <algorithm>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace util { namespace stream { namespace {

struct CompareUInt64 {
  bool operator()(const void *first, const void *second) const {
    return *static_cast<const uint64_t*>(first) < *reinterpret_cast<const uint64_t*>(second);
  }
};

const uint64_t kSize = 100000;

struct Putter {
  Putter(std::vector<uint64_t> &shuffled) : shuffled_(shuffled) {}

  void Run(const ChainPosition &position) {
    Stream put_shuffled(position);
    for (uint64_t i = 0; i < shuffled_.size(); ++i, ++put_shuffled) {
      *static_cast<uint64_t*>(put_shuffled.Get()) = shuffled_[i];
    }
    put_shuffled.Poison();
  }
  std::vector<uint64_t> &shuffled_;
};

TEST_CASE("FromShuffled") {
  std::vector<uint64_t> shuffled;
  shuffled.reserve(kSize);
  for (uint64_t i = 0; i < kSize; ++i) {
    shuffled.push_back(i);
  }
  std::shuffle(shuffled.begin(), shuffled.end(), std::mt19937(std::random_device{}()));

  ChainConfig config;
  config.entry_size = 8;
  config.total_memory = 800;
  config.block_count = 3;

  SortConfig merge_config;
  merge_config.temp_prefix = "sort_test_temp";
  merge_config.buffer_size = 800;
  merge_config.total_memory = 3300;

  Chain chain(config);
  chain >> Putter(shuffled);
  BlockingSort(chain, merge_config, CompareUInt64(), NeverCombine());
  Stream sorted;
  chain >> sorted >> kRecycle;
  for (uint64_t i = 0; i < kSize; ++i, ++sorted) {
    CHECK_EQ(i, *static_cast<const uint64_t*>(sorted.Get()));
  }
  CHECK(!sorted);
}

}}} // namespaces

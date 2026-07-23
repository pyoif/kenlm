#include "adjust_counts.hh"

#include "../common/ngram_stream.hh"
#include "payload.hh"
#include "../../util/scoped.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

namespace lm { namespace builder { namespace {

class KeepCopy {
  public:
    KeepCopy() : size_(0) {}

    void Run(const util::stream::ChainPosition &position) {
      for (util::stream::Link link(position); link; ++link) {
        mem_.call_realloc(size_ + link->ValidSize());
        memcpy(static_cast<uint8_t*>(mem_.get()) + size_, link->Get(), link->ValidSize());
        size_ += link->ValidSize();
      }
    }

    uint8_t *Get() { return static_cast<uint8_t*>(mem_.get()); }
    std::size_t Size() const { return size_; }

  private:
    util::scoped_malloc mem_;
    std::size_t size_;
};

struct Gram4 {
  WordIndex ids[4];
  uint64_t count;
};

class WriteInput {
  public:
    void Run(const util::stream::ChainPosition &position) {
      NGramStream<BuildingPayload> input(position);
      Gram4 grams[] = {
        {{0,0,0,0},10},
        {{0,0,3,0},3},
        // bos
        {{1,1,1,2},5},
        {{0,0,3,2},5},
      };
      for (size_t i = 0; i < sizeof(grams) / sizeof(Gram4); ++i, ++input) {
        memcpy(input->begin(), grams[i].ids, sizeof(WordIndex) * 4);
        input->Value().count = grams[i].count;
      }
      input.Poison();
    }
};

TEST_CASE("Simple") {
  KeepCopy outputs[4];
  std::vector<uint64_t> counts;
  std::vector<Discount> discount;
  {
    util::stream::ChainConfig config;
    config.total_memory = 100;
    config.block_count = 1;
    util::stream::Chains chains(4);
    for (unsigned i = 0; i < 4; ++i) {
      config.entry_size = NGram<BuildingPayload>::TotalSize(i + 1);
      chains.push_back(config);
    }

    chains[3] >> WriteInput();
    util::stream::ChainPositions for_adjust(chains);
    for (unsigned i = 0; i < 4; ++i) {
      chains[i] >> std::ref(outputs[i]);
    }
    chains >> util::stream::kRecycle;
    std::vector<uint64_t> counts_pruned(4);
    std::vector<uint64_t> prune_thresholds(4);
    DiscountConfig discount_config;
    discount_config.fallback = Discount();
    discount_config.bad_action = THROW_UP;
    CHECK_THROWS_AS(AdjustCounts(prune_thresholds, counts, counts_pruned, std::vector<bool>(), discount_config, discount).Run(for_adjust), BadDiscountException);
  }
  REQUIRE_EQ(4UL, counts.size());
  CHECK_EQ(4UL, counts[0]);
  // These are no longer set because the discounts are bad.
/*  CHECK_EQ(4UL, counts[1]);
  CHECK_EQ(3UL, counts[2]);
  CHECK_EQ(3UL, counts[3]);*/
  REQUIRE_EQ(NGram<BuildingPayload>::TotalSize(1) * 4, outputs[0].Size());
  NGram<BuildingPayload> uni(outputs[0].Get(), 1);
  CHECK_EQ(kUNK, *uni.begin());
  CHECK_EQ(0ULL, uni.Value().count);
  uni.NextInMemory();
  CHECK_EQ(kBOS, *uni.begin());
  CHECK_EQ(0ULL, uni.Value().count);
  uni.NextInMemory();
  CHECK_EQ(0UL, *uni.begin());
  CHECK_EQ(2ULL, uni.Value().count);
  uni.NextInMemory();
  CHECK_EQ(2ULL, uni.Value().count);
  CHECK_EQ(2UL, *uni.begin());

  REQUIRE_EQ(NGram<BuildingPayload>::TotalSize(2) * 4, outputs[1].Size());
  NGram<BuildingPayload> bi(outputs[1].Get(), 2);
  CHECK_EQ(0UL, *bi.begin());
  CHECK_EQ(0UL, *(bi.begin() + 1));
  CHECK_EQ(1ULL, bi.Value().count);
  bi.NextInMemory();
}

}}} // namespaces

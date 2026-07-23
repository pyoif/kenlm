#include "normalize.hh"

#include "interpolate_info.hh"
#include "merge_probabilities.hh"
#include "../common/ngram_stream.hh"
#include "../../util/stream/chain.hh"
#include "../../util/stream/multi_stream.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

namespace lm { namespace interpolate { namespace {

// log without backoff
const float kInputs[] = {-0.3, 1.2, -9.8, 4.0, -7.0, 0.0};

class WriteInput {
  public:
    WriteInput() {}
    void Run(const util::stream::ChainPosition &to) {
      util::stream::Stream out(to);
      for (WordIndex i = 0; i < sizeof(kInputs) / sizeof(float); ++i, ++out) {
        memcpy(out.Get(), &i, sizeof(WordIndex));
        memcpy((uint8_t*)out.Get() + sizeof(WordIndex), &kInputs[i], sizeof(float));
      }
      out.Poison();
    }
};

void CheckOutput(const util::stream::ChainPosition &from) {
  NGramStream<float> in(from);
  float sum = 0.0;
  for (WordIndex i = 0; i < sizeof(kInputs) / sizeof(float) - 1 /* <s> at the end */; ++i) {
    sum += pow(10.0, kInputs[i]);
  }
  sum = log10(sum);
  REQUIRE(in);
  CHECK(static_cast<double>(kInputs[0] - sum) == doctest::Approx(static_cast<double>(in->Value())).epsilon(static_cast<double>(0.0001) / 100.0));
  REQUIRE(++in);
  CHECK(static_cast<double>(kInputs[1] - sum) == doctest::Approx(static_cast<double>(in->Value())).epsilon(static_cast<double>(0.0001) / 100.0));
  REQUIRE(++in);
  CHECK(static_cast<double>(kInputs[2] - sum) == doctest::Approx(static_cast<double>(in->Value())).epsilon(static_cast<double>(0.0001) / 100.0));
  REQUIRE(++in);
  CHECK(static_cast<double>(kInputs[3] - sum) == doctest::Approx(static_cast<double>(in->Value())).epsilon(static_cast<double>(0.0001) / 100.0));
  REQUIRE(++in);
  CHECK(static_cast<double>(kInputs[4] - sum) == doctest::Approx(static_cast<double>(in->Value())).epsilon(static_cast<double>(0.0001) / 100.0));
  REQUIRE(++in);
  CHECK(static_cast<double>(kInputs[5] - sum) == doctest::Approx(static_cast<double>(in->Value())).epsilon(static_cast<double>(0.0001) / 100.0));
  CHECK_FALSE(++in);
}

TEST_CASE("Unigrams") {
  InterpolateInfo info;
  info.lambdas.push_back(2.0);
  info.lambdas.push_back(-0.1);
  info.orders.push_back(1);
  info.orders.push_back(1);

  CHECK_EQ(0, MakeEncoder(info, 1).EncodedLength());

  // No backoffs.
  util::stream::Chains blank(0);
  util::FixedArray<util::stream::ChainPositions> models_by_order(2);
  models_by_order.push_back(blank);
  models_by_order.push_back(blank);

  util::stream::Chains merged_probabilities(1);
  util::stream::Chains probabilities_out(1);
  util::stream::Chains backoffs_out(0);

  merged_probabilities.push_back(util::stream::ChainConfig(sizeof(WordIndex) + sizeof(float) + sizeof(float), 2, 24));
  probabilities_out.push_back(util::stream::ChainConfig(sizeof(WordIndex) + sizeof(float), 2, 100));

  merged_probabilities[0] >> WriteInput();
  Normalize(info, models_by_order, merged_probabilities, probabilities_out, backoffs_out);

  util::stream::ChainPosition checker(probabilities_out[0].Add());

  merged_probabilities >> util::stream::kRecycle;
  probabilities_out >> util::stream::kRecycle;

  CheckOutput(checker);
  probabilities_out.Wait();
}

}}} // namespaces

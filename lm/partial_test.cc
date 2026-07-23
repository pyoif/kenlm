#include "partial.hh"

#include "left.hh"
#include "model.hh"
#include "../util/tokenize_piece.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#define CHECK_CLOSE(ref, value, tol) CHECK(static_cast<double>(ref) == doctest::Approx(static_cast<double>(value)).epsilon(static_cast<double>(tol) / 100.0))


namespace lm {
namespace ngram {
namespace {

const char *TestLocation() { return "test.arpa"; }

Config SilentConfig() {
  Config config;
  config.arpa_complain = Config::NONE;
  config.messages = NULL;
  return config;
}

struct ModelFixture {
  ModelFixture() : m(TestLocation(), SilentConfig()) {}

  RestProbingModel m;
};

BOOST_FIXTURE_TEST_SUITE(suite, ModelFixture)

TEST_CASE("SimpleBefore") {
  Left left;
  left.full = false;
  left.length = 0;
  Right right;
  right.length = 0;

  Right reveal;
  reveal.length = 1;
  WordIndex period = m.GetVocabulary().Index(".");
  reveal.words[0] = period;
  reveal.backoff[0] = -0.845098;

  CHECK_CLOSE(0.0, RevealBefore(m, reveal, 0, false, left, right), 0.001);
  CHECK_EQ(0, left.length);
  CHECK(!left.full);
  CHECK_EQ(1, right.length);
  CHECK_EQ(period, right.words[0]);
  CHECK_CLOSE(-0.845098, right.backoff[0], 0.001);

  WordIndex more = m.GetVocabulary().Index("more");
  reveal.words[1] = more;
  reveal.backoff[1] =  -0.4771212;
  reveal.length = 2;
  CHECK_CLOSE(0.0, RevealBefore(m, reveal, 1, false, left, right), 0.001);
  CHECK_EQ(0, left.length);
  CHECK(!left.full);
  CHECK_EQ(2, right.length);
  CHECK_EQ(period, right.words[0]);
  CHECK_EQ(more, right.words[1]);
  CHECK_CLOSE(-0.845098, right.backoff[0], 0.001);
  CHECK_CLOSE(-0.4771212, right.backoff[1], 0.001);
}

TEST_CASE("AlsoWouldConsider") {
  WordIndex would = m.GetVocabulary().Index("would");
  WordIndex consider = m.GetVocabulary().Index("consider");

  ChartState current;
  current.left.length = 1;
  current.left.pointers[0] = would;
  current.left.full = false;
  current.right.length = 1;
  current.right.words[0] = would;
  current.right.backoff[0] = -0.30103;

  Left after;
  after.full = false;
  after.length = 1;
  after.pointers[0] = consider;

  // adjustment for would consider
  CHECK_CLOSE(-1.687872 - -0.2922095 - 0.30103, RevealAfter(m, current.left, current.right, after, 0), 0.001);

  CHECK_EQ(2, current.left.length);
  CHECK_EQ(would, current.left.pointers[0]);
  CHECK_EQ(false, current.left.full);

  WordIndex also = m.GetVocabulary().Index("also");
  Right before;
  before.length = 1;
  before.words[0] = also;
  before.backoff[0] = -0.30103;
  // r(would) = -0.2922095 [i would], r(would -> consider) = -1.988902 [b(would) + p(consider)]
  // p(also -> would) = -2, p(also would -> consider) = -3
  CHECK_CLOSE(-2 + 0.2922095 -3 + 1.988902, RevealBefore(m, before, 0, false, current.left, current.right), 0.001);
  CHECK_EQ(0, current.left.length);
  CHECK(current.left.full);
  CHECK_EQ(2, current.right.length);
  CHECK_EQ(would, current.right.words[0]);
  CHECK_EQ(also, current.right.words[1]);
}

TEST_CASE("EndSentence") {
  WordIndex loin = m.GetVocabulary().Index("loin");
  WordIndex period = m.GetVocabulary().Index(".");
  WordIndex eos = m.GetVocabulary().EndSentence();

  ChartState between;
  between.left.length = 1;
  between.left.pointers[0] = eos;
  between.left.full = true;
  between.right.length = 0;

  Right before;
  before.words[0] = period;
  before.words[1] = loin;
  before.backoff[0] = -0.845098;
  before.backoff[1] = 0.0;

  before.length = 1;
  CHECK_CLOSE(-0.0410707, RevealBefore(m, before, 0, true, between.left, between.right), 0.001);
  CHECK_EQ(0, between.left.length);
}

float ScoreFragment(const RestProbingModel &model, unsigned int *begin, unsigned int *end, ChartState &out) {
  RuleScore<RestProbingModel> scorer(model, out);
  for (unsigned int *i = begin; i < end; ++i) {
    scorer.Terminal(*i);
  }
  return scorer.Finish();
}

void CheckAdjustment(const RestProbingModel &model, float expect, const Right &before_in, bool before_full, ChartState between, const Left &after_in) {
  Right before(before_in);
  Left after(after_in);
  after.full = false;
  float got = 0.0;
  for (unsigned int i = 1; i < 5; ++i) {
    if (before_in.length >= i) {
      before.length = i;
      got += RevealBefore(model, before, i - 1, false, between.left, between.right);
    }
    if (after_in.length >= i) {
      after.length = i;
      got += RevealAfter(model, between.left, between.right, after, i - 1);
    }
  }
  if (after_in.full) {
    after.full = true;
    got += RevealAfter(model, between.left, between.right, after, after.length);
  }
  if (before_full) {
    got += RevealBefore(model, before, before.length, true, between.left, between.right);
  }
  // Sometimes they're zero and CHECK_CLOSE fails for this.
  CHECK(fabs(expect - got) < 0.001);
}

void FullDivide(const RestProbingModel &model, StringPiece str) {
  std::vector<WordIndex> indices;
  for (util::TokenIter<util::SingleCharacter, true> i(str, ' '); i; ++i) {
    indices.push_back(model.GetVocabulary().Index(*i));
  }
  ChartState full_state;
  float full = ScoreFragment(model, &indices.front(), &indices.back() + 1, full_state);

  ChartState before_state;
  before_state.left.full = false;
  RuleScore<RestProbingModel> before_scorer(model, before_state);
  float before_score = 0.0;
  for (unsigned int before = 0; before < indices.size(); ++before) {
    for (unsigned int after = before; after <= indices.size(); ++after) {
      ChartState after_state, between_state;
      float after_score = ScoreFragment(model, &indices.front() + after, &indices.front() + indices.size(), after_state);
      float between_score = ScoreFragment(model, &indices.front() + before, &indices.front() + after, between_state);
      CheckAdjustment(model, full - before_score - after_score - between_score, before_state.right, before_state.left.full, between_state, after_state.left);
    }
    before_scorer.Terminal(indices[before]);
    before_score = before_scorer.Finish();
  }
}

TEST_CASE("Strings") {
  FullDivide(m, "also would consider");
  FullDivide(m, "looking on a little more loin . </s>");
  FullDivide(m, "in biarritz watching considering looking . on a little more loin also would consider higher to look good unknown the screening foo bar , unknown however unknown </s>");
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace
} // namespace ngram
} // namespace lm

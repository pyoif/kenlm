#include "partial.hh"

#include "left.hh"
#include "model.hh"
#include "../util/tokenize_piece.hh"

#include <doctest/doctest.h>
#include "../util/test_main.hh"

namespace lm {
namespace ngram {
namespace {

const char *TestLocation() {
  if (test_argc < 2) {
    return "test.arpa";
  }

static RestProbingModel &GetModel() {
  static RestProbingModel m(TestLocation(), SilentConfig());
  return m;
}
  return test_argv[1];
}

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

TEST_CASE("SimpleBefore") {
  RestProbingModel &m = GetModel();
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

  CHECK(static_cast<double>(0.0) == doctest::Approx(static_cast<double>(RevealBefore(m, reveal, 0, false, left)).epsilon(static_cast<double>(right) / 100.0)), 0.001);
  CHECK_EQ(0, left.length);
  CHECK_FALSE(left.full);
  CHECK_EQ(1, right.length);
  CHECK_EQ(period, right.words[0]);
  CHECK(static_cast<double>(-0.845098) == doctest::Approx(static_cast<double>(right.backoff[0])).epsilon(static_cast<double>(0.001) / 100.0));

  WordIndex more = m.GetVocabulary().Index("more");
  reveal.words[1] = more;
  reveal.backoff[1] =  -0.4771212;
  reveal.length = 2;
  CHECK(static_cast<double>(0.0) == doctest::Approx(static_cast<double>(RevealBefore(m, reveal, 1, false, left)).epsilon(static_cast<double>(right) / 100.0)), 0.001);
  CHECK_EQ(0, left.length);
  CHECK_FALSE(left.full);
  CHECK_EQ(2, right.length);
  CHECK_EQ(period, right.words[0]);
  CHECK_EQ(more, right.words[1]);
  CHECK(static_cast<double>(-0.845098) == doctest::Approx(static_cast<double>(right.backoff[0])).epsilon(static_cast<double>(0.001) / 100.0));
  CHECK(static_cast<double>(-0.4771212) == doctest::Approx(static_cast<double>(right.backoff[1])).epsilon(static_cast<double>(0.001) / 100.0));
}

TEST_CASE("AlsoWouldConsider") {
  RestProbingModel &m = GetModel();
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
  CHECK(static_cast<double>(-1.687872 - -0.2922095 - 0.30103) == doctest::Approx(static_cast<double>(RevealAfter(m, current.left, current.right, after)).epsilon(static_cast<double>(0) / 100.0)), 0.001);

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
  CHECK(static_cast<double>(-2 + 0.2922095 -3 + 1.988902) == doctest::Approx(static_cast<double>(RevealBefore(m, before, 0, false, current.left)).epsilon(static_cast<double>(current.right) / 100.0)), 0.001);
  CHECK_EQ(0, current.left.length);
  CHECK(current.left.full);
  CHECK_EQ(2, current.right.length);
  CHECK_EQ(would, current.right.words[0]);
  CHECK_EQ(also, current.right.words[1]);
}

TEST_CASE("EndSentence") {
  RestProbingModel &m = GetModel();
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
  CHECK(static_cast<double>(-0.0410707) == doctest::Approx(static_cast<double>(RevealBefore(m, before, 0, true, between.left)).epsilon(static_cast<double>(between.right) / 100.0)), 0.001);
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
  // Sometimes they're zero and BOOST_CHECK_CLOSE fails for this.
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
  RestProbingModel &m = GetModel();
  FullDivide(m, "also would consider");
  FullDivide(m, "looking on a little more loin . </s>");
  FullDivide(m, "in biarritz watching considering looking . on a little more loin also would consider higher to look good unknown the screening foo bar , unknown however unknown </s>");
}

} // namespace
} // namespace ngram
} // namespace lm

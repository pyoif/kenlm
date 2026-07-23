#include "model_buffer.hh"
#include "../model.hh"
#include "../state.hh"

#include <doctest/doctest.h>
#include "../../util/test_main.hh"

namespace lm { namespace {

TEST_CASE("Query") {
  std::string dir("test_data");
  if (test_argc == 2) {
    dir = test_argv[1];
  }
  ngram::Model ref((dir + "/toy0.arpa").c_str());
#if BYTE_ORDER == LITTLE_ENDIAN
  std::string endian = "little";
#elif BYTE_ORDER == BIG_ENDIAN
  std::string endian = "big";
#else
#error "Unsupported byte order."
#endif

  ModelBuffer test(dir + "/" + endian + "endian/toy0");
  ngram::State ref_state, test_state;
  WordIndex a = ref.GetVocabulary().Index("a");
  CHECK(static_cast<double>(ref.FullScore(ref.BeginSentenceState()) == doctest::Approx(static_cast<double>(a)).epsilon(static_cast<double>(ref_state) / 100.0)).prob,
      test.SlowQuery(ref.BeginSentenceState(), a, test_state),
      0.001);
  CHECK_EQ((unsigned)ref_state.length, (unsigned)test_state.length);
  CHECK_EQ(ref_state.words[0], test_state.words[0]);
  CHECK_EQ(ref_state.backoff[0], test_state.backoff[0]);
  CHECK(ref_state == test_state);

  ngram::State ref_state2, test_state2;
  WordIndex b = ref.GetVocabulary().Index("b");
  CHECK(static_cast<double>(ref.FullScore(ref_state) == doctest::Approx(static_cast<double>(b)).epsilon(static_cast<double>(ref_state2) / 100.0)).prob,
      test.SlowQuery(test_state, b, test_state2),
      0.001);
  CHECK(ref_state2 == test_state2);
  CHECK_EQ(ref_state2.backoff[0], test_state2.backoff[0]);

  CHECK(static_cast<double>(ref.FullScore(ref_state2) == doctest::Approx(static_cast<double>(0)).epsilon(static_cast<double>(ref_state) / 100.0)).prob,
      test.SlowQuery(test_state2, 0, test_state),
      0.001);
  // The reference does state minimization but this doesn't.
}

}} // namespaces

#include "tune_derivatives.hh"

#include "tune_instances.hh"

#include "../../util/stream/config.hh"
#include "../../util/stream/chain.hh"
#include "../../util/stream/io.hh"
#include "../../util/stream/typed_stream.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

namespace lm { namespace interpolate {

class MockInstances : public Instances {
  public:
    MockInstances() : chain_(util::stream::ChainConfig(ReadExtensionsEntrySize(), 2, 100)), write_(chain_.Add()) {
      extensions_subsequent_.reset(new util::stream::FileBuffer(util::MakeTemp("/tmp/")));
      chain_ >> extensions_subsequent_->Sink() >> util::stream::kRecycle;
    }

    Matrix &LNUnigrams() { return ln_unigrams_; }

    BackoffMatrix &LNBackoffs() { return ln_backoffs_; }

    WordIndex &BOS() { return bos_; }

    Vector &NegLNCorrectSum() { return neg_ln_correct_sum_; }

    // Extensions must be provided sorted!
    void AddExtension(const Extension &extension) {
      *write_ = extension;
      ++write_;
    }

    void DoneExtending() {
      write_.Poison();
      chain_.Wait(true);
    }

  private:
    util::stream::Chain chain_;
    util::stream::TypedStream<Extension> write_;
};

namespace {

TEST_CASE("Small") {
  MockInstances mock;

  {
    // Three vocabulary words plus <s>, two models.
    Matrix unigrams(4, 2);
    unigrams <<
      0.1, 0.6,
      0.4, 0.3,
      0.5, 0.1,
      // <s>
      1.0, 1.0;
    mock.LNUnigrams() = unigrams.array().log();
  }
  mock.BOS() = 3;

  // One instance
  mock.LNBackoffs().resize(1, 2);
  mock.LNBackoffs() << 0.2, 0.4;
  mock.LNBackoffs() = mock.LNBackoffs().array().log();

  // Sparse extensions: model 0 word 2 and model 1 word 1.

  // Assuming that model 1 only matches word 1, this is p_1(1 | context)
  Accum model_1_word_1 = 1.0 - .6 * .4 - .1 * .4;

  mock.NegLNCorrectSum().resize(2);
  // We'll suppose correct has WordIndex 1, which backs off in model 0, and matches in model 1
  mock.NegLNCorrectSum() << (0.4 * 0.2), model_1_word_1;
  mock.NegLNCorrectSum() = -mock.NegLNCorrectSum().array().log();

  Accum model_0_word_2 = 1.0 - .1 * .2 - .4 * .2;

  Extension ext;

  ext.instance = 0;
  ext.word = 1;
  ext.model = 1;
  ext.ln_prob = log(model_1_word_1);
  mock.AddExtension(ext);

  ext.instance = 0;
  ext.word = 2;
  ext.model = 0;
  ext.ln_prob = log(model_0_word_2);
  mock.AddExtension(ext);

  mock.DoneExtending();

  Vector weights(2);
  weights << 0.9, 1.2;

  Vector gradient(2);
  Matrix hessian(2,2);
  Derivatives(mock, weights, gradient, hessian);
  // TODO: check perplexity value coming out.

  // p_I(x | context)
  Vector p_I(3);
  p_I <<
    pow(0.1 * 0.2, 0.9) * pow(0.6 * 0.4, 1.2),
    pow(0.4 * 0.2, 0.9) * pow(model_1_word_1, 1.2),
    pow(model_0_word_2, 0.9) * pow(0.1 * 0.4, 1.2);
  p_I /= p_I.sum();

  Vector expected_gradient = mock.NegLNCorrectSum();
  expected_gradient(0) += p_I(0) * log(0.1 * 0.2);
  expected_gradient(0) += p_I(1) * log(0.4 * 0.2);
  expected_gradient(0) += p_I(2) * log(model_0_word_2);
  CHECK(static_cast<double>(expected_gradient(0)) == doctest::Approx(static_cast<double>(gradient(0))).epsilon(static_cast<double>(0.01) / 100.0));

  expected_gradient(1) += p_I(0) * log(0.6 * 0.4);
  expected_gradient(1) += p_I(1) * log(model_1_word_1);
  expected_gradient(1) += p_I(2) * log(0.1 * 0.4);
  CHECK(static_cast<double>(expected_gradient(1)) == doctest::Approx(static_cast<double>(gradient(1))).epsilon(static_cast<double>(0.01) / 100.0));

  Matrix expected_hessian(2, 2);
  expected_hessian(1, 0) =
    // First term
    p_I(0) * log(0.1 * 0.2) * log(0.6 * 0.4) +
    p_I(1) * log(0.4 * 0.2) * log(model_1_word_1) +
    p_I(2) * log(model_0_word_2) * log(0.1 * 0.4);
  expected_hessian(1, 0) -=
    (p_I(0) * log(0.1 * 0.2) + p_I(1) * log(0.4 * 0.2) + p_I(2) * log(model_0_word_2)) *
    (p_I(0) * log(0.6 * 0.4) + p_I(1) * log(model_1_word_1) + p_I(2) * log(0.1 * 0.4));
  expected_hessian(0, 1) = expected_hessian(1, 0);
  CHECK(static_cast<double>(expected_hessian(1) == doctest::Approx(static_cast<double>(0), hessian(1)).epsilon(static_cast<double>(0) / 100.0)), 0.01);
  CHECK(static_cast<double>(expected_hessian(0) == doctest::Approx(static_cast<double>(1), hessian(0)).epsilon(static_cast<double>(1) / 100.0)), 0.01);
}

}}} // namespaces

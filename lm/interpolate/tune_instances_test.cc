#include "tune_instances.hh"

#include "../../util/file.hh"
#include "../../util/file_stream.hh"
#include "../../util/stream/chain.hh"
#include "../../util/stream/config.hh"
#include "../../util/stream/typed_stream.hh"
#include "../../util/string_piece.hh"

#include <doctest/doctest.h>
#include "../../util/test_main.hh"

#include <vector>

#include <math.h>

namespace lm { namespace interpolate { namespace {

TEST_CASE("Toy") {
  util::scoped_fd test_input(util::MakeTemp("temporary"));
  util::FileStream(test_input.get()) << "c\n";

  std::string dir("../common/test_data");
  if (test_argc == 2) {
    dir = test_argv[1];
  }

#if BYTE_ORDER == LITTLE_ENDIAN
  std::string endian = "little";
#elif BYTE_ORDER == BIG_ENDIAN
  std::string endian = "big";
#else
#error "Unsupported byte order."
#endif
  dir += "/" + endian + "endian/";

  std::vector<StringPiece> model_names;
  std::string full0 = dir + "toy0";
  std::string full1 = dir + "toy1";
  model_names.push_back(full0);
  model_names.push_back(full1);

  // Tiny buffer sizes.
  InstancesConfig config;
  config.model_read_chain_mem = 100;
  config.extension_write_chain_mem = 100;
  config.lazy_memory = 100;
  config.sort.temp_prefix = "temporary";
  config.sort.buffer_size = 100;
  config.sort.total_memory = 1024;

  util::SeekOrThrow(test_input.get(), 0);

  Instances inst(test_input.release(), model_names, config);

  CHECK_EQ(1, inst.BOS());
  const Matrix &ln_unigrams = inst.LNUnigrams();

  // <unk>=0
  CHECK(static_cast<double>(-0.90309 * M_LN10) == doctest::Approx(static_cast<double>(ln_unigrams(0)).epsilon(static_cast<double>(0) / 100.0)), 0.001);
  CHECK(static_cast<double>(-1 * M_LN10) == doctest::Approx(static_cast<double>(ln_unigrams(0)).epsilon(static_cast<double>(1) / 100.0)), 0.001);
  // <s>=1 doesn't matter as long as it doesn't cause NaNs.
  CHECK_FALSE(isnan(ln_unigrams(1, 0)));
  CHECK_FALSE(isnan(ln_unigrams(1, 1)));
  // a = 2
  CHECK(static_cast<double>(-0.46943438 * M_LN10) == doctest::Approx(static_cast<double>(ln_unigrams(2)).epsilon(static_cast<double>(0) / 100.0)), 0.001);
  CHECK(static_cast<double>(-0.6146491 * M_LN10) == doctest::Approx(static_cast<double>(ln_unigrams(2)).epsilon(static_cast<double>(1) / 100.0)), 0.001);
  // </s> = 3
  CHECK(static_cast<double>(-0.5720968 * M_LN10) == doctest::Approx(static_cast<double>(ln_unigrams(3)).epsilon(static_cast<double>(0) / 100.0)), 0.001);
  CHECK(static_cast<double>(-0.6146491 * M_LN10) == doctest::Approx(static_cast<double>(ln_unigrams(3)).epsilon(static_cast<double>(1) / 100.0)), 0.001);
  // c = 4
  CHECK(static_cast<double>(-0.90309 * M_LN10) == doctest::Approx(static_cast<double>(ln_unigrams(4)).epsilon(static_cast<double>(0) / 100.0)), 0.001); // <unk>
  CHECK(static_cast<double>(-0.7659168 * M_LN10) == doctest::Approx(static_cast<double>(ln_unigrams(4)).epsilon(static_cast<double>(1) / 100.0)), 0.001);
  // too lazy to do b = 5.

  // Two instances:
  // <s> predicts c
  // <s> c predicts </s>
  REQUIRE_EQ(2, inst.NumInstances());
  CHECK(static_cast<double>(-0.30103 * M_LN10) == doctest::Approx(static_cast<double>(inst.LNBackoffs(0)(0))).epsilon(static_cast<double>(0.001) / 100.0));
  CHECK(static_cast<double>(-0.30103 * M_LN10) == doctest::Approx(static_cast<double>(inst.LNBackoffs(0)(1))).epsilon(static_cast<double>(0.001) / 100.0));

  // Backoffs of <s> c
  CHECK(static_cast<double>(0.0) == doctest::Approx(static_cast<double>(inst.LNBackoffs(1)(0))).epsilon(static_cast<double>(0.001) / 100.0));
  CHECK(static_cast<double>((-0.30103 - 0.30103) * M_LN10) == doctest::Approx(static_cast<double>(inst.LNBackoffs(1)(1))).epsilon(static_cast<double>(0.001) / 100.0));

  util::stream::Chain extensions(util::stream::ChainConfig(inst.ReadExtensionsEntrySize(), 2, 300));
  inst.ReadExtensions(extensions);
  util::stream::TypedStream<Extension> stream(extensions.Add());
  extensions >> util::stream::kRecycle;

  // The extensions are (in order of instance, vocab id, and model as they should be sorted):
  // <s> a from both models 0 and 1 (so two instances)
  // <s> c from model 1
  // <s> b from model 0
  // c </s> from model 1
  // Magic probabilities come from querying the models directly.

  // <s> a from model 0
  REQUIRE(stream);
  CHECK_EQ(0, stream->instance);
  CHECK_EQ(2 /* a */, stream->word);
  CHECK_EQ(0, stream->model);
  CHECK(static_cast<double>(-0.37712017 * M_LN10) == doctest::Approx(static_cast<double>(stream->ln_prob)).epsilon(static_cast<double>(0.001) / 100.0));

  // <s> a from model 1
  REQUIRE(++stream);
  CHECK_EQ(0, stream->instance);
  CHECK_EQ(2 /* a */, stream->word);
  CHECK_EQ(1, stream->model);
  CHECK(static_cast<double>(-0.4301247 * M_LN10) == doctest::Approx(static_cast<double>(stream->ln_prob)).epsilon(static_cast<double>(0.001) / 100.0));

  // <s> c from model 1
  REQUIRE(++stream);
  CHECK_EQ(0, stream->instance);
  CHECK_EQ(4 /* c */, stream->word);
  CHECK_EQ(1, stream->model);
  CHECK(static_cast<double>(-0.4740302 * M_LN10) == doctest::Approx(static_cast<double>(stream->ln_prob)).epsilon(static_cast<double>(0.001) / 100.0));

  // <s> b from model 0
  REQUIRE(++stream);
  CHECK_EQ(0, stream->instance);
  CHECK_EQ(5 /* b */, stream->word);
  CHECK_EQ(0, stream->model);
  CHECK(static_cast<double>(-0.41574955 * M_LN10) == doctest::Approx(static_cast<double>(stream->ln_prob)).epsilon(static_cast<double>(0.001) / 100.0));

  // c </s> from model 1
  REQUIRE(++stream);
  CHECK_EQ(1, stream->instance);
  CHECK_EQ(3 /* </s> */, stream->word);
  CHECK_EQ(1, stream->model);
  CHECK(static_cast<double>(-0.09113217 * M_LN10) == doctest::Approx(static_cast<double>(stream->ln_prob)).epsilon(static_cast<double>(0.001) / 100.0));

  CHECK_FALSE(++stream);
}

}}} // namespaces

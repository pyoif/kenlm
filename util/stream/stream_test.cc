#include "io.hh"

#include "stream.hh"
#include "../file.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <unistd.h>

namespace util { namespace stream { namespace {

TEST_CASE("StreamTest") {
  scoped_fd in(MakeTemp("io_test_temp"));
  for (uint64_t i = 0; i < 100000; ++i) {
    WriteOrThrow(in.get(), &i, sizeof(uint64_t));
  }
  SeekOrThrow(in.get(), 0);

  ChainConfig config;
  config.entry_size = 8;
  config.total_memory = 100;
  config.block_count = 12;

  Stream s;
  Chain chain(config);
  chain >> Read(in.get()) >> s >> kRecycle;
  uint64_t i = 0;
  for (; s; ++s, ++i) {
    CHECK_EQ(i, *static_cast<const uint64_t*>(s.Get()));
  }
  CHECK_EQ(100000ULL, i);
}

}}} // namespaces

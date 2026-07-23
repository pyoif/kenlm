#include "io.hh"

#include "chain.hh"
#include "../file.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace util { namespace stream { namespace {

TEST_CASE("CopyFile") {
  std::string temps("io_test_temp");

  scoped_fd in(MakeTemp(temps));
  for (uint64_t i = 0; i < 100000; ++i) {
    WriteOrThrow(in.get(), &i, sizeof(uint64_t));
  }
  SeekOrThrow(in.get(), 0);
  scoped_fd out(MakeTemp(temps));

  ChainConfig config;
  config.entry_size = 8;
  config.total_memory = 1024;
  config.block_count = 10;

  Chain(config) >> PRead(in.get()) >> Write(out.get());

  SeekOrThrow(out.get(), 0);
  for (uint64_t i = 0; i < 100000; ++i) {
    uint64_t got;
    ReadOrThrow(out.get(), &got, sizeof(uint64_t));
    CHECK_EQ(i, got);
  }
}

}}} // namespaces

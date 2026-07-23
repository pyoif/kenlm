#include "read_compressed.hh"

#include "file.hh"
#include "have.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <memory>

#include <fstream>
#include <string>
#include <cstdlib>

#if defined(_WIN32) || defined(_WIN64)
#include <ctime>
#include <fcntl.h>
#include <io.h>

#if !defined(mkstemp)
inline int mkstemp(char * stemplate) {
    char *filename = _mktemp(stemplate);
    if (filename == NULL) return -1;
    return _open(filename, _O_RDWR | _O_CREAT, 0600);
}
#endif

#endif // defined

namespace util {
namespace {

void ReadLoop(ReadCompressed &reader, void *to_void, std::size_t amount) {
  uint8_t *to = static_cast<uint8_t*>(to_void);
  while (amount) {
    std::size_t ret = reader.Read(to, amount);
    REQUIRE(ret);
    to += ret;
    amount -= ret;
  }
}

const uint32_t kSize4 = 100000 / 4;

std::string WriteRandom() {
  char name[] = "tempXXXXXX";
  util::scoped_fd original(mkstemp(name));
  REQUIRE(original.get() > 0);
  for (uint32_t i = 0; i < kSize4; ++i) {
    WriteOrThrow(original.get(), &i, sizeof(uint32_t));
  }
  return name;
}

void VerifyRead(ReadCompressed &reader) {
  for (uint32_t i = 0; i < kSize4; ++i) {
    uint32_t got;
    ReadLoop(reader, &got, sizeof(uint32_t));
    CHECK_EQ(i, got);
  }

  char ignored;
  CHECK_EQ((std::size_t)0, reader.Read(&ignored, 1));
  // Test double EOF call.
  CHECK_EQ((std::size_t)0, reader.Read(&ignored, 1));
}

void TestRandom(const char *compressor) {
  std::string name(WriteRandom());

  char gzname[] = "tempXXXXXX";
  util::scoped_fd gzipped(mkstemp(gzname));

  std::string command(compressor);
#ifdef __CYGWIN__
  command += ".exe";
#endif
  command += " <\"";
  command += name;
  command += "\" >\"";
  command += gzname;
  command += "\"";
  REQUIRE_EQ(0, system(command.c_str()));

  CHECK_EQ(0, unlink(name.c_str()));
  CHECK_EQ(0, unlink(gzname));

  ReadCompressed reader(gzipped.release());
  VerifyRead(reader);
}

TEST_CASE("Uncompressed") {
  TestRandom("cat");
}

#ifdef HAVE_ZLIB
TEST_CASE("ReadGZ") {
  TestRandom("gzip");
}
#endif // HAVE_ZLIB

#ifdef HAVE_BZLIB
TEST_CASE("ReadBZ") {
  TestRandom("bzip2");
}
#endif // HAVE_BZLIB

#ifdef HAVE_XZLIB
TEST_CASE("ReadXZ") {
  TestRandom("xz");
}
#endif

#ifdef HAVE_ZLIB
TEST_CASE("AppendGZ") {
}
#endif

TEST_CASE("IStream") {
  std::string name(WriteRandom());
  std::fstream stream(name.c_str(), std::ios::in);
  CHECK_EQ(0, unlink(name.c_str()));
  ReadCompressed reader;
  reader.Reset(stream);
  VerifyRead(reader);
}

} // namespace
} // namespace util

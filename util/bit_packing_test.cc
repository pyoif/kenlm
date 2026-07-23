#include "bit_packing.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstring>

namespace util {
namespace {

const uint64_t test57 = 0x123456789abcdefULL;
const uint32_t test25 = 0x1234567;

TEST_CASE("ZeroBit57") {
  char mem[16];
  memset(mem, 0, sizeof(mem));
  WriteInt57(mem, 0, 57, test57);
  CHECK_EQ(test57, ReadInt57(mem, 0, 57, (1ULL << 57) - 1));
}

TEST_CASE("EachBit57") {
  char mem[16];
  for (uint8_t b = 0; b < 8; ++b) {
    memset(mem, 0, sizeof(mem));
    WriteInt57(mem, b, 57, test57);
    CHECK_EQ(test57, ReadInt57(mem, b, 57, (1ULL << 57) - 1));
  }
}

TEST_CASE("Consecutive57") {
  char mem[57+8];
  memset(mem, 0, sizeof(mem));
  for (uint64_t b = 0; b < 57 * 8; b += 57) {
    WriteInt57(mem, b, 57, test57);
    CHECK_EQ(test57, ReadInt57(mem, b, 57, (1ULL << 57) - 1));
  }
  for (uint64_t b = 0; b < 57 * 8; b += 57) {
    CHECK_EQ(test57, ReadInt57(mem, b, 57, (1ULL << 57) - 1));
  }
}

TEST_CASE("Consecutive25") {
  char mem[25+8];
  memset(mem, 0, sizeof(mem));
  for (uint64_t b = 0; b < 25 * 8; b += 25) {
    WriteInt25(mem, b, 25, test25);
    CHECK_EQ(test25, ReadInt25(mem, b, 25, (1ULL << 25) - 1));
  }
  for (uint64_t b = 0; b < 25 * 8; b += 25) {
    CHECK_EQ(test25, ReadInt25(mem, b, 25, (1ULL << 25) - 1));
  }
}

TEST_CASE("Sanity") {
  BitPackingSanity();
}

} // namespace
} // namespace util

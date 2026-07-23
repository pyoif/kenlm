#include "tokenize_piece.hh"
#include "string_piece.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <iostream>

namespace util {
namespace {

TEST_CASE("pipe_pipe_none") {
  const char str[] = "nodelimit at all";
  TokenIter<MultiCharacter> it(str, MultiCharacter("|||"));
  REQUIRE(it);
  CHECK_EQ(StringPiece(str), *it);
  ++it;
  CHECK_FALSE(it);
}
TEST_CASE("pipe_pipe_two") {
  const char str[] = "|||";
  TokenIter<MultiCharacter> it(str, MultiCharacter("|||"));
  REQUIRE(it);
  CHECK_EQ(StringPiece(), *it);
  ++it;
  REQUIRE(it);
  CHECK_EQ(StringPiece(), *it);
  ++it;
  CHECK_FALSE(it);
}

TEST_CASE("remove_empty") {
  const char str[] = "|||";
  TokenIter<MultiCharacter, true> it(str, MultiCharacter("|||"));
  CHECK_FALSE(it);
}

TEST_CASE("remove_empty_keep") {
  const char str[] = " |||";
  TokenIter<MultiCharacter, true> it(str, MultiCharacter("|||"));
  REQUIRE(it);
  CHECK_EQ(StringPiece(" "), *it);
  ++it;
  CHECK_FALSE(it);
}

} // namespace
} // namespace util

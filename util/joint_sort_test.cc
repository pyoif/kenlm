#include "joint_sort.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

namespace util { namespace {

TEST_CASE("just_flip") {
  char keys[2];
  int values[2];
  keys[0] = 1; values[0] = 327;
  keys[1] = 0; values[1] = 87897;
  JointSort<char *, int *>(keys + 0, keys + 2, values + 0);
  CHECK_EQ(0, keys[0]);
  CHECK_EQ(87897, values[0]);
  CHECK_EQ(1, keys[1]);
  CHECK_EQ(327, values[1]);
}

TEST_CASE("three") {
  char keys[3];
  int values[3];
  keys[0] = 1; values[0] = 327;
  keys[1] = 2; values[1] = 87897;
  keys[2] = 0; values[2] = 10;
  JointSort<char *, int *>(keys + 0, keys + 3, values + 0);
  CHECK_EQ(0, keys[0]);
  CHECK_EQ(1, keys[1]);
  CHECK_EQ(2, keys[2]);
}

TEST_CASE("char_int") {
  char keys[4];
  int values[4];
  keys[0] = 3; values[0] = 327;
  keys[1] = 1; values[1] = 87897;
  keys[2] = 2; values[2] = 10;
  keys[3] = 0; values[3] = 24347;
  JointSort<char *, int *>(keys + 0, keys + 4, values + 0);
  CHECK_EQ(0, keys[0]);
  CHECK_EQ(24347, values[0]);
  CHECK_EQ(1, keys[1]);
  CHECK_EQ(87897, values[1]);
  CHECK_EQ(2, keys[2]);
  CHECK_EQ(10, values[2]);
  CHECK_EQ(3, keys[3]);
  CHECK_EQ(327, values[3]);
}

TEST_CASE("swap_proxy") {
  char keys[2] = {0, 1};
  int values[2] = {2, 3};
  detail::JointProxy<char *, int *> first(keys, values);
  detail::JointProxy<char *, int *> second(keys + 1, values + 1);
  swap(first, second);
  CHECK_EQ(1, keys[0]);
  CHECK_EQ(0, keys[1]);
  CHECK_EQ(3, values[0]);
  CHECK_EQ(2, values[1]);
}

}} // namespace anonymous util

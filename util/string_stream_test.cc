#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "string_stream.hh"
#include <doctest/doctest.h>
#include <cstddef>
#include <limits>
#include <sstream>

namespace util { namespace {

template <class T> std::string Stringify(const T &value) {
  std::ostringstream ss;
  ss << value;
  return ss.str();
}

std::string Stringify(const void *value) {
  std::ostringstream ss;
  ss << value;
  return ss.str();
}

template <class T> void TestEqual(const T value) {
  StringStream strme;
  strme << value;
  { auto expected = Stringify(value); CHECK_EQ(expected, strme.str()); }
}

template <class T> void TestCorners() {
  TestEqual(std::numeric_limits<T>::max());
  TestEqual(std::numeric_limits<T>::min());
  TestEqual(static_cast<T>(0));
  TestEqual(static_cast<T>(-1));
  TestEqual(static_cast<T>(1));
}

TEST_CASE("Integer") {
  TestCorners<char>();
  TestCorners<signed char>();
  TestCorners<unsigned char>();

  TestCorners<short>();
  TestCorners<signed short>();
  TestCorners<unsigned short>();

  TestCorners<int>();
  TestCorners<unsigned int>();
  TestCorners<signed int>();

  TestCorners<long>();
  TestCorners<unsigned long>();
  TestCorners<signed long>();

  TestCorners<long long>();
  TestCorners<unsigned long long>();
  TestCorners<signed long long>();

  TestCorners<std::size_t>();
}

enum TinyEnum { EnumValue };

TEST_CASE("EnumCase") {
  TestEqual(EnumValue);
}

TEST_CASE("Strings") {
  TestEqual("foo");
  const char *a = "bar";
  TestEqual(a);
  StringPiece piece("abcdef");
  TestEqual(piece);
  TestEqual(StringPiece());

  char non_const[3];
  non_const[0] = 'b';
  non_const[1] = 'c';
  non_const[2] = 0;

  StringStream out;
  out << "a" << non_const << 'c';
  CHECK_EQ("abcc", out.str());

  // Now test as a separate object.
  StringStream stream;
  stream << "a" << non_const << 'c' << piece;
  CHECK_EQ("abccabcdef", stream.str());
}

}} // namespaces

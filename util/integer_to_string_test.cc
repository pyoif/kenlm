#include "integer_to_string.hh"
#include "string_piece.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cctype>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

namespace util {
namespace {

template <class T> std::string ToStr(const T &value) {
  std::ostringstream ss;
  ss << value;
  return ss.str();
}

template <class T> void TestValue(const T value) {
  char buf[ToStringBuf<T>::kBytes];
  StringPiece result(buf, ToString(value, buf) - buf);
  REQUIRE_GE(static_cast<std::size_t>(ToStringBuf<T>::kBytes), result.size());
  if (value) {
    std::string expected = ToStr(value);
    std::string got(result.data(), result.size());
    bool match = (expected == got);
    CHECK(match);
  } else {
    bool is_zero = (result == "0x0") || (result == "0");
    CHECK(is_zero);
  }
}

template <> void TestValue<const void*>(const void* value) {
  char buf[ToStringBuf<const void*>::kBytes];
  StringPiece result(buf, ToString(value, buf) - buf);
  REQUIRE_GE(static_cast<std::size_t>(ToStringBuf<const void*>::kBytes), result.size());
  if (value) {
    std::string expected = ToStr(value);
    std::string got(result.data(), result.size());
    // MSVC ostream << void*: zero-padded uppercase hex without 0x (e.g. "0000000000000001")
    // ToString: 0x + lowercase hex without padding (e.g. "0x1")
    // Normalize both: lowercase, strip 0x from got, strip leading zeros from expected.
    std::string expected_norm = expected;
    std::string got_norm = got;
    for (char &c : expected_norm) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char &c : got_norm) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (got_norm.size() >= 2 && got_norm[0] == '0' && got_norm[1] == 'x')
      got_norm = got_norm.substr(2);
    size_t pos = expected_norm.find_first_not_of('0');
    if (pos != std::string::npos)
      expected_norm = expected_norm.substr(pos);
    CHECK(expected_norm == got_norm);
  } else {
    bool is_zero = (result == "0x0") || (result == "0");
    CHECK(is_zero);
  }
}

template <class T> void TestCorners() {
  TestValue(std::numeric_limits<T>::min());
  TestValue(std::numeric_limits<T>::max());
  TestValue((T)0);
  TestValue((T)-1);
  TestValue((T)1);
}

TEST_CASE("Corners") {
  TestCorners<uint16_t>();
  TestCorners<uint32_t>();
  TestCorners<uint64_t>();
  TestCorners<int16_t>();
  TestCorners<int32_t>();
  TestCorners<int64_t>();
  TestCorners<const void*>();
}

template <class T> void TestAll() {
  for (T i = std::numeric_limits<T>::min(); i < std::numeric_limits<T>::max(); ++i) {
    TestValue(i);
  }
  TestValue(std::numeric_limits<T>::max());
}

TEST_CASE("Short") {
  TestAll<uint16_t>();
  TestAll<int16_t>();
}

template <class T> void Test10s() {
  for (T i = 1; i < std::numeric_limits<T>::max() / 10; i *= 10) {
    TestValue(i);
    TestValue(i - 1);
    TestValue(i + 1);
  }
}

TEST_CASE("Tens") {
  Test10s<uint64_t>();
  Test10s<int64_t>();
  Test10s<uint32_t>();
  Test10s<int32_t>();
}

TEST_CASE("Pointers") {
  for (uintptr_t i = 1; i < std::numeric_limits<uintptr_t>::max() / 10; i *= 10) {
    TestValue((const void*)i);
  }
  for (uintptr_t i = 0; i < 256; ++i) {
    TestValue((const void*)i);
    TestValue((const void*)(i + 0xf00));
  }
}

}} // namespaces

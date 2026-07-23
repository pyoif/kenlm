#include "probing_hash_table.hh"

#include "murmur_hash.hh"
#include "scoped.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <memory>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>

namespace util {
namespace {

struct Entry {
  unsigned char key;
  typedef unsigned char Key;

  unsigned char GetKey() const {
    return key;
  }

  void SetKey(unsigned char to) {
    key = to;
  }

  uint64_t GetValue() const {
    return value;
  }

  uint64_t value;
};

typedef ProbingHashTable<Entry, std::hash<unsigned char> > Table;

TEST_CASE("simple") {
  size_t size = Table::Size(10, 1.2);
  std::unique_ptr<char[]> mem(new char[size]);
  memset(mem.get(), 0, size);

  Table table(mem.get(), size);
  const Entry *i = NULL;
  CHECK_FALSE(table.Find(2, i));
  Entry to_ins;
  to_ins.key = 3;
  to_ins.value = 328920;
  table.Insert(to_ins);
  REQUIRE(table.Find(3, i));
  CHECK_EQ(3, i->GetKey());
  CHECK_EQ(static_cast<uint64_t>(328920), i->GetValue());
  CHECK_FALSE(table.Find(2, i));
}

struct Entry64 {
  uint64_t key;
  typedef uint64_t Key;

  Entry64() {}

  explicit Entry64(uint64_t key_in) {
    key = key_in;
  }

  Key GetKey() const { return key; }
  void SetKey(uint64_t to) { key = to; }
};

struct MurmurHashEntry64 {
  std::size_t operator()(uint64_t value) const {
    return util::MurmurHash64A(&value, 8);
  }
};

typedef ProbingHashTable<Entry64, MurmurHashEntry64> Table64;

TEST_CASE("Double") {
  for (std::size_t initial = 19; initial < 30; ++initial) {
    size_t size = Table64::Size(initial, 1.2);
    scoped_malloc mem(MallocOrThrow(size));
    Table64 table(mem.get(), size, std::numeric_limits<uint64_t>::max());
    table.Clear();
    for (uint64_t i = 0; i < 19; ++i) {
      table.Insert(Entry64(i));
    }
    table.CheckConsistency();
    mem.call_realloc(table.DoubleTo());
    table.Double(mem.get());
    table.CheckConsistency();
    for (uint64_t i = 20; i < 40 ; ++i) {
      table.Insert(Entry64(i));
    }
    mem.call_realloc(table.DoubleTo());
    table.Double(mem.get());
    table.CheckConsistency();
  }
}

} // namespace
} // namespace util

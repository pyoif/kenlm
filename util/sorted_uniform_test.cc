#include "sorted_uniform.hh"

#include <random>
#include <memory>
#include <unordered_map>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <limits>
#include <vector>

namespace util {
namespace {

template <class KeyT, class ValueT> struct Entry {
  typedef KeyT Key;
  typedef ValueT Value;

  Key key;
  Value value;

  Key GetKey() const {
    return key;
  }

  Value GetValue() const {
    return value;
  }

  bool operator<(const Entry<Key,Value> &other) const {
    return key < other.key;
  }
};

template <class KeyT> struct Accessor {
  typedef KeyT Key;
  template <class Value> Key operator()(const Entry<Key, Value> *entry) const {
    return entry->GetKey();
  }
};

template <class Key, class Value> void Check(const Entry<Key, Value> *begin, const Entry<Key, Value> *end, const std::unordered_map<Key, Value> &reference, const Key key) {
  typename std::unordered_map<Key, Value>::const_iterator ref = reference.find(key);
  typedef const Entry<Key, Value> *It;
  // g++ can't tell that require will crash and burn.
  It i = NULL;
  bool ret = SortedUniformFind<It, Accessor<Key>, Pivot64>(Accessor<Key>(), begin, end, key, i);
  if (ref == reference.end()) {
    CHECK_FALSE(ret);
  } else {
    REQUIRE(ret);
    CHECK_EQ(ref->second, i->GetValue());
  }
}

TEST_CASE("empty") {
  typedef const Entry<uint64_t, float> T;
  const T *i;
  bool ret = SortedUniformFind<const T*, Accessor<uint64_t>, Pivot64>(Accessor<uint64_t>(), (const T*)NULL, (const T*)NULL, (uint64_t)10, i);
  CHECK_FALSE(ret);
}

template <class Key> void RandomTest(unsigned int upper, size_t entries, size_t queries) {
  typedef unsigned char Value;
  std::mt19937 rng;
  std::uniform_int_distribution<unsigned int> range_key(0, upper);
  std::uniform_int_distribution<int> range_value(0, 255);
  auto gen_key = [&]() { return range_key(rng); };
  auto gen_value = [&]() { return range_value(rng); };

  typedef Entry<Key, Value> Ent;
  std::vector<Ent> backing;
  std::unordered_map<Key, unsigned char> reference;
  Ent ent;
  for (size_t i = 0; i < entries; ++i) {
    Key key = gen_key();
    unsigned char value = gen_value();
    if (reference.insert(std::make_pair(key, value)).second) {
      ent.key = key;
      ent.value = value;
      backing.push_back(ent);
    }
  }
  std::sort(backing.begin(), backing.end());

  // Random queries.
  for (size_t i = 0; i < queries; ++i) {
    const Key key = gen_key();
    Check<Key, unsigned char>(&*backing.begin(), &*backing.end(), reference, key);
  }

  typename std::unordered_map<Key, unsigned char>::const_iterator it = reference.begin();
  for (size_t i = 0; (i < queries) && (it != reference.end()); ++i, ++it) {
    Check<Key, unsigned char>(&*backing.begin(), &*backing.end(), reference, it->second);
  }
}

TEST_CASE("basic") {
  RandomTest<uint8_t>(11, 10, 200);
}

TEST_CASE("tiny_dense_random") {
  RandomTest<uint8_t>(11, 50, 200);
}

TEST_CASE("small_dense_random") {
  RandomTest<uint8_t>(100, 100, 200);
}

TEST_CASE("small_sparse_random") {
  RandomTest<uint8_t>(200, 15, 200);
}

TEST_CASE("medium_sparse_random") {
  RandomTest<uint16_t>(32000, 1000, 2000);
}

TEST_CASE("sparse_random") {
  RandomTest<uint64_t>(std::numeric_limits<uint64_t>::max(), 100000, 2000);
}

} // namespace
} // namespace util

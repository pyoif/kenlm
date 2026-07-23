# Design: Remove All Boost Dependencies from kenlm

**Date:** 2025-07-23  
**Status:** Approved  
**Goal:** Eliminate all boost dependencies. Replace with header-only CLI11 + doctest, C++17 std equivalents. Bump language standard to C++17.

---

## Summary

kenlm currently depends on boost in four categories:

1. **`boost::program_options`** — CLI argument parsing in 7 binaries + 1 header
2. **`boost::test`** — Unit test framework in 23 test files + 3 files using `floating_point_comparison`
3. **Boost-only utilities replaced by C++17** — `optional`, `noncopyable`, `in_place_factory`
4. **Boost utilities trivially replaced by C++11** — `unordered_map`, `unordered_set`, `shared_ptr`, `scoped_ptr`, `scoped_array`, `functional/hash`, `random`, `lexical_cast`, `version.hpp`, `range/iterator_range`

Replacements:
- `boost::program_options` → **CLI11** (v2.4.2, vendored single header, BSD-3-Clause)
- `boost::test` → **doctest** (v2.4.11, vendored single header, MIT)
- `boost::optional` → `std::optional` (C++17)
- `boost::noncopyable` → `= delete` (C++11, now applied)
- `boost::in_place_factory` → direct construction
- All other misc → C++11 `std` equivalents

**Zero compiled library dependencies.** Two vendored headers. C++17 baseline.

---

## Section 0: Language Standard Bump — C++11 → C++17

**CMake:**
```cmake
set(CMAKE_CXX_STANDARD 17)
```

**Rationale:** C++17 enables `std::optional` (replaces `boost::optional`). C++17 compilers universally available: GCC 8+, Clang 5+, MSVC 2017 15.7+. No code changes needed for existing C++11 usage — C++17 is a superset.

---

## Section 1: CLI Replacement — `boost::program_options` → CLI11

### Files affected (8)

| File | Role |
|---|---|
| `lm/builder/count_ngrams_main.cc` | `count_ngrams` binary |
| `lm/builder/lmplz_main.cc` | `lmplz` binary |
| `lm/kenlm_benchmark_main.cc` | `kenlm_benchmark` binary |
| `lm/interpolate/interpolate_main.cc` | `interpolate` binary |
| `lm/interpolate/streaming_example_main.cc` | streaming example binary |
| `lm/interpolate/tune_weights.cc` | tuning binary |
| `lm/common/size_option.cc` | shared `SizeOption` helper |
| `lm/common/size_option.hh` | remove `#include <boost/program_options.hpp>` |

### Migration pattern

```cpp
// Before
#include <boost/program_options.hpp>
namespace po = boost::program_options;
po::options_description options("desc");
options.add_options()
  ("help,h", po::bool_switch(), "...")
  ("order,o", po::value<unsigned>(&order)->required(), "...");
po::variables_map vm;
po::store(po::parse_command_line(argc, argv, options), vm);
po::notify(vm);
if (vm["help"].as<bool>()) { ... }

// After
#include <CLI/CLI.hpp>
CLI::App app{"desc"};
bool help = false;
app.add_flag("-h,--help", help, "...");
app.add_option("-o,--order", order, "...")->required();
CLI11_PARSE(app, argc, argv);
if (help) { ... }
```

### Special cases

**`size_option` (shared helper):** `boost::program_options::typed_value` with custom notifier. CLI11 equivalent uses `->transform(SizeNotify)` on the option. `SizeNotify` signature adjusts from `void(std::string)` notifier to `std::size_t(std::string)` transform.

**Multitoken vector options (`--prune`, `--discount_fallback`):** CLI11 `app.add_option("--prune", pruning)->expected(-1)` for variable-length vectors.

**Explicit/implicit bool (`--interpolate_unigrams`):** CLI11 `->default_val(true)` handles the default.

**`BOOST_VERSION` ifdefs:** `lmplz_main.cc` line 87/144 guards against Boost < 1.42 (2010). Remove `#include <boost/version.hpp>`, remove conditionals, keep `#if BOOST_VERSION >= 104200` path unconditionally.

**`streaming_example_main.cc`:** Also includes `boost/version.hpp` — remove.

### CLI11 delivery

Vendor `CLI11.hpp` into `third_party/CLI11/include/CLI/CLI.hpp`. Version pinned at 2.4.2 (last release with C++11 support; fully compatible with C++17). No CMake build step — just add include path to affected targets.

### CMake changes

- Add include path for CLI11 to all binary targets that use it
- Remove `find_package(Boost ... program_options)` and associated `Boost_INCLUDE_DIRS`

---

## Section 2: Test Framework — `boost::test` → doctest

### Framework choice

**doctest** (v2.4.11, MIT license) — single header, fastest compile of all C++ test frameworks, Catch2-like syntax. Vendored alongside CLI11. No CMake FetchContent needed.

### Files affected (26)

All `*_test.cc` files in: `lm/builder/`, `lm/common/`, `lm/interpolate/`, `lm/`, `util/`, `util/stream/`

Plus 3 files using `boost/test/floating_point_comparison.hpp`: `lm/left_test.cc`, `lm/model_test.cc`, `lm/partial_test.cc`

### Migration pattern

| Boost.Test | doctest |
|---|---|
| `#define BOOST_TEST_MODULE Name` | `#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` (in one source per test binary) |
| `#include <boost/test/included/unit_test.hpp>` | `#include <doctest/doctest.h>` |
| `BOOST_AUTO_TEST_CASE(Name) { }` | `TEST_CASE("Name") { }` |
| `BOOST_CHECK_EQUAL(a, b)` | `CHECK_EQ(a, b)` |
| `BOOST_REQUIRE_EQUAL(a, b)` | `REQUIRE_EQ(a, b)` |
| `BOOST_CHECK_CLOSE(a, b, tol)` | `CHECK(a == doctest::Approx(b).epsilon(tol / 100.0))` |
| `BOOST_CHECK(a)` | `CHECK(a)` |
| `BOOST_CHECK(!a)` | `CHECK_FALSE(a)` |
| `BOOST_CHECK_THROW(stmt, ex)` | `CHECK_THROWS_AS(stmt, ex)` |
| `BOOST_REQUIRE(stream)` | `REQUIRE(stream)` |

### doctest Approx for CLOSE semantics

`BOOST_CHECK_CLOSE(a, b, 0.1)` means "a within 0.1% of b" (relative). doctest:

```cpp
CHECK(a == doctest::Approx(b).epsilon(0.001));  // 0.1% = 0.001 epsilon
```

If tolerance > 0.1% needed in some tests, adjust epsilon accordingly.

### Test main strategy

Each test `.cc` file currently compiles as its own binary (one call to `KenLMAddTest` per test). Each file currently has `#define BOOST_TEST_MODULE X` before include. For doctest: replace with `#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` in each test file, then `#include <doctest/doctest.h>`. Each test binary remains self-contained.

### Suite naming

Original `BOOST_AUTO_TEST_CASE` name becomes `TEST_CASE("name")`. No explicit suite needed — doctest can optionally use `TEST_CASE("name" * doctest::description("desc"))` or group with `SUBCASE`.

### doctest delivery

Vendor `doctest.h` into `third_party/doctest/include/doctest/doctest.h`. No CMake build step — just include path. Add to `KenLMFunctions.cmake` test target includes.

### CMake changes — KenLMFunctions.cmake

- Remove `Boost_USE_STATIC_LIBS` / `BOOST_TEST_DYN_LINK` logic block
- Remove `DYNLINK_FLAGS` variable and its use in `set_target_properties`
- Add doctest include path to test targets
- Remove gtest / boost test lib from link list — doctest is header-only, no lib to link

### CMake changes — subdirectory CMakeLists.txt

- Rename all `KENLM_BOOST_TESTS_LIST` → `KENLM_TESTS_LIST`
- Remove `${Boost_LIBRARIES}` from `LIBRARIES` entries (e.g., `util/stream/CMakeLists.txt` line 38)

---

## Section 3: Utility Replacements — boost → C++17 std

### C++17-unlocked replacements

#### `boost::optional` → `std::optional` (2 files)

| File | Changes |
|---|---|
| `util/multi_intersection.hh` | `#include <boost/optional.hpp>` → `#include <optional>`. `boost::optional<Value>` → `std::optional<Value>`. `return boost::optional<Value>()` → `return std::nullopt`. `return boost::optional<Value>(v)` → `return v`. Also replace `boost::iterator_range` with local template (see Range section below). |
| `lm/interpolate/tune_instances.hh` | `#include <boost/optional.hpp>` → `#include <optional>`. Replace any `boost::optional` usage. |

#### `boost::noncopyable` → `= delete` (5 files)

```cpp
// Before
class Stream : boost::noncopyable { ... };

// After  
class Stream {
  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;
  ...
};
```

Files: `util/stream/stream.hh`, `util/stream/rewindable_stream.hh`, `lm/filter/arpa_io.hh`, `lm/filter/vocab.hh`, `lm/filter/thread.hh`

#### `boost::in_place_factory` (1 file)

`lm/filter/thread.hh` uses `boost::in_place(boost::ref(x), boost::ref(y))` for queue element construction. Per C++17, replace with direct construction. Check what the queue constructor expects — if it takes variadic args, pass them directly without wrapper.

### C++11-trivial replacements

#### `boost::unordered_map` / `boost::unordered_set` (5 files + 1 header)

`#include <boost/unordered_map.hpp>` → `#include <unordered_map>`. `boost::unordered_map<K,V>` → `std::unordered_map<K,V>`. Same for `unordered_set`.

Files: `lm/filter/phrase_table_vocab_main.cc`, `lm/interpolate/tune_instances.cc`, `lm/filter/vocab.cc`, `lm/filter/vocab.hh`, `lm/filter/phrase.hh`, `lm/builder/corpus_count.cc` (uses boost::scoped_array, plus check for unordered_map)

#### `boost::shared_ptr` (1 file)

`lm/interpolate/tune_instances.cc`: `boost::shared_ptr<T>` → `std::shared_ptr<T>`. `boost::make_shared` → `std::make_shared`.

#### `boost::scoped_ptr` (1 file)

`util/read_compressed_test.cc`: `boost::scoped_ptr<FILE>` → `std::unique_ptr<FILE>`.

#### `boost::scoped_array` (3 files)

`lm/builder/corpus_count.cc`, `util/probing_hash_table_test.cc`, `util/sorted_uniform_test.cc`, plus header `lm/filter/arpa_io.hh`: `boost::scoped_array<T>` → `std::unique_ptr<T[]>`. Array indexing works identically.

#### `boost::functional/hash` (1 file)

`util/probing_hash_table_test.cc`: `boost::hash<uint64_t>` → `std::hash<uint64_t>`.

#### `boost::random` (1 file)

`util/sorted_uniform_test.cc`:
```cpp
// Before
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
boost::mt19937 gen;
boost::uniform_int<> dist(0, max_value);
boost::variate_generator<boost::mt19937&, boost::uniform_int<>> rng(gen, dist);

// After
#include <random>
std::mt19937 gen;
std::uniform_int_distribution<int> dist(0, max_value);
// Use dist(gen) directly — no variate_generator
```

#### `boost::lexical_cast` (5 files + 1 header)

| File | Replacement |
|---|---|
| `lm/builder/dump_counts_main.cc` | `std::stoi`, `std::stod` for primitives |
| `lm/common/model_buffer.cc` | `std::stod` |
| `lm/builder/debug_print.hh` | `std::to_string` or stream insert |
| `util/integer_to_string_test.cc` | `std::to_string` |
| `util/string_stream_test.cc` | `std::stod`, `std::to_string` |
| `util/integer_to_string.cc` | check usage if any |

#### `boost/version.hpp` (2 files)

`lm/builder/lmplz_main.cc`, `lm/interpolate/streaming_example_main.cc`: Remove include. `#if BOOST_VERSION >= 104200` unconditionally true — keep "then" branch only.

#### `boost/range/iterator_range` (4 files + 1 new header)

Replaced by new reusable header `util/range.hh`:

```cpp
#pragma once

#include <cstddef>
#include <functional>

namespace util {

template <class I>
struct Range {
  I begin_;
  I end_;

  I begin() const { return begin_; }
  I end() const { return end_; }
  bool empty() const { return begin_ == end_; }
  std::size_t size() const { return end_ - begin_; }

  typename I::reference front() const { return *begin_; }
  typename I::reference back() const { return *(end_ - 1); }

  void advance_begin(std::size_t n) { begin_ += n; }
};

} // namespace util
```

**Files using `boost::iterator_range`:**

| File | Replacement |
|---|---|
| `lm/kenlm_benchmark_main.cc` | `boost::iterator_range<Width*>` → `util::Range<Width*>` |
| `lm/filter/vocab.hh` | `boost::iterator_range<const unsigned int*>` → `util::Range<const unsigned int*>` |
| `util/multi_intersection.hh` | All `boost::iterator_range<Iterator>` → `util::Range<Iterator>`. Also `#include <boost/range/iterator_range.hpp>` → `#include "util/range.hh"` |

---

## Section 4: CMake Global Cleanup

### Root CMakeLists.txt

Remove:
- Lines 2-8: `CMP0167` policy (was needed only for FindBoost)
- Lines 9-17: WIN32 `BOOST_*` defines block (`BOOST_ALL_NO_LIB`, `BOOST_PROGRAM_OPTIONS_DYN_LINK`, etc.)
- Lines 109-121: `find_package(Boost ...)` and `include_directories(${Boost_INCLUDE_DIRS})`

Change:
- `CMAKE_CXX_STANDARD 11` → `CMAKE_CXX_STANDARD 17`

Add:
- `target_include_directories` for `third_party/CLI11/include` and `third_party/doctest/include` as needed

### cmake/kenlmConfig.cmake.in

Remove:
```
find_dependency(Boost)
```
Keep `find_dependency(Threads)`.

### cmake/KenLMFunctions.cmake

- Remove `Boost_USE_STATIC_LIBS` / `BOOST_TEST_DYN_LINK` logic block (lines 42-45)
- Remove `DYNLINK_FLAGS` from `set_target_properties` call
- Add doctest include path to test targets

### Subdirectory CMakeLists.txt

- **`util/CMakeLists.txt`:** Remove `"$<BUILD_INTERFACE:${Boost_LIBRARIES}>"` from `kenlm_util` PUBLIC link. Remove comment "Boost is required...".
- **`util/stream/CMakeLists.txt`:** Remove `${Boost_LIBRARIES}` from test LIBRARIES.
- All subdirectories: rename `KENLM_BOOST_TESTS_LIST` → `KENLM_TESTS_LIST`.

---

## Section 5: Error Handling & Edge Cases

- **CLI11 parse errors:** Prints help and exits with non-zero by default — same behavior as program_options.
- **CLI11 positional args:** `count_ngrams` and `lmplz` accept positional text file input. CLI11 supports `app.add_option("input", ...)->check(CLI::ExistingFile)` for optional positional.
- **doctest `Approx` vs `BOOST_CHECK_CLOSE`:** `CLOSE(a, b, 0.1)` = 0.1% relative tolerance. doctest `Approx(b).epsilon(0.001)` gives 0.1%. Verify tolerance semantics match — `CLOSE` does `abs(a-b)/max(abs(a),abs(b)) < tol/100`.
- **`util::Range` API compatibility:** `boost::iterator_range` has `.begin()`, `.end()`, `.empty()`, `.size()`, `.front()`, `.back()`, `.advance_begin(n)`. Our Range must support all these.
- **Backwards compatibility:** No API change to kenlm library. Build system + tool internals only.
- **CI:** Remove boost from CI dependency install. No replacement needed — doctest and CLI11 are vendored.

---

## Section 6: Files Summary

### New files
- `third_party/CLI11/include/CLI/CLI.hpp` — vendored CLI11 v2.4.2
- `third_party/doctest/include/doctest/doctest.h` — vendored doctest v2.4.11
- `util/range.hh` — simple Range template (C++17)

### Modified files (~55 total)

**CLI migration (8):** `count_ngrams_main.cc`, `lmplz_main.cc`, `kenlm_benchmark_main.cc`, `interpolate_main.cc`, `streaming_example_main.cc`, `tune_weights.cc`, `size_option.cc`, `size_option.hh`

**Test migration (26):** All `*_test.cc` files + 3 floating_point_comparison removals

**Utility — optional (2):** `multi_intersection.hh`, `tune_instances.hh`

**Utility — noncopyable (5):** `stream.hh`, `rewindable_stream.hh`, `arpa_io.hh`, `vocab.hh`, `thread.hh`

**Utility — in_place (1):** `thread.hh`

**Utility — unordered (6):** `phrase_table_vocab_main.cc`, `tune_instances.cc`, `vocab.cc`, `vocab.hh`, `phrase.hh`, `corpus_count.cc`

**Utility — shared_ptr (1):** `tune_instances.cc`

**Utility — scoped_ptr (1):** `read_compressed_test.cc`

**Utility — scoped_array (4):** `corpus_count.cc`, `arpa_io.hh`, `probing_hash_table_test.cc`, `sorted_uniform_test.cc`

**Utility — hash (1):** `probing_hash_table_test.cc`

**Utility — random (1):** `sorted_uniform_test.cc`

**Utility — lexical_cast (6):** `dump_counts_main.cc`, `debug_print.hh`, `model_buffer.cc`, `integer_to_string_test.cc`, `string_stream_test.cc`, `integer_to_string.cc`

**Utility — version.hpp (2):** `lmplz_main.cc`, `streaming_example_main.cc`

**Utility — iterator_range (3):** `kenlm_benchmark_main.cc`, `vocab.hh`, `multi_intersection.hh`

**CMake (6):** `CMakeLists.txt` (root), `KenLMFunctions.cmake`, `kenlmConfig.cmake.in`, `lm/builder/CMakeLists.txt`, `util/CMakeLists.txt`, `util/stream/CMakeLists.txt`

### Deleted files
- None (boost headers were system-installed, not vendored)

---

## Section 7: Testing Strategy

1. Build with `COMPILE_TESTS=ON` — all 27+ test binaries compile
2. `ctest` — all tests pass
3. Manual smoke: `lmplz --help`, `count_ngrams --help`, `kenlm_benchmark --help`, `interpolate --help`
4. Regression: `lmplz` produces identical ARPA output on sample corpus
5. CI: remove boost from CI dependencies (`.github/workflows/`). No replacement — doctest + CLI11 are vendored.

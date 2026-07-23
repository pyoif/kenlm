# Remove All Boost Dependencies — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate all boost dependencies from kenlm by vendoring CLI11 + doctest and migrating to C++17 std equivalents.

**Architecture:** Two vendored headers (CLI11 v2.4.2, doctest v2.4.11) in `third_party/`. New `util/range.hh` template replaces `boost::iterator_range`. C++17 bump enables `std::optional`. All other boost usage maps to C++11/17 std. CMake build system simplified — no more `find_package(Boost)`.

**Tech Stack:** C++17, CMake 3.28+, CLI11 2.4.2 (header-only), doctest 2.4.11 (header-only)

## Global Constraints

- C++17 language standard (`CMAKE_CXX_STANDARD 17`)
- CLI11 v2.4.2 vendored at `third_party/CLI11/include/CLI/CLI.hpp`
- doctest v2.4.11 vendored at `third_party/doctest/include/doctest/doctest.h`
- `util/range.hh` provides `util::Range<T>` replacing `boost::iterator_range`
- No compiled library dependencies introduced
- Backwards compatibility: no API change to kenlm library
- `boost::noncopyable` → `= delete` on copy ctor and copy assignment
- `boost::optional<T>` → `std::optional<T>` with `std::nullopt`
- `BOOST_CHECK_CLOSE(a, b, tol)` → `CHECK(a == doctest::Approx(b).epsilon(tol / 100.0))`

---

## File Structure

### New files
```
third_party/CLI11/include/CLI/CLI.hpp          — vendored CLI11 v2.4.2 (download from GitHub release)
third_party/doctest/include/doctest/doctest.h  — vendored doctest v2.4.11 (download from GitHub release)
util/range.hh                                   — util::Range<I> template
```

### Modified files — CMake (6)
```
CMakeLists.txt                     — root: remove boost, C++17, add third_party includes
cmake/KenLMFunctions.cmake         — remove BOOST_TEST_DYN_LINK, add doctest includes
cmake/kenlmConfig.cmake.in         — remove find_dependency(Boost)
lm/builder/CMakeLists.txt          — rename KENLM_BOOST_TESTS_LIST → KENLM_TESTS_LIST
util/CMakeLists.txt                — remove Boost_LIBRARIES ref from kenlm_util
util/stream/CMakeLists.txt         — remove Boost_LIBRARIES, rename test list var
```

### Modified files — CLI migration (8)
```
lm/common/size_option.hh           — remove boost include, change signature
lm/common/size_option.cc           — CLI11 transform-based SizeOption
lm/builder/count_ngrams_main.cc    — program_options → CLI11
lm/builder/lmplz_main.cc           — program_options → CLI11, BOOST_VERSION ifdefs, lexical_cast
lm/kenlm_benchmark_main.cc         — program_options → CLI11, iterator_range → util::Range
lm/interpolate/interpolate_main.cc — program_options → CLI11 (MungeWeightArgs)
lm/interpolate/streaming_example_main.cc — program_options → CLI11, version.hpp
lm/interpolate/tune_weights.cc     — program_options remove (dead include, no actual po usage)
```

### Modified files — doctest migration (26)
```
All *_test.cc files in: lm/builder/, lm/common/, lm/interpolate/, lm/, util/, util/stream/
Plus floating_point_comparison removal in: lm/left_test.cc, lm/model_test.cc, lm/partial_test.cc
```

### Modified files — utility replacements (~25)
```
util/multi_intersection.hh         — optional + iterator_range → std::optional + util::Range
lm/interpolate/tune_instances.hh   — optional
lm/interpolate/tune_instances.cc   — shared_ptr + unordered_map
util/stream/stream.hh              — noncopyable
util/stream/rewindable_stream.hh   — noncopyable
lm/filter/arpa_io.hh               — noncopyable + scoped_array
lm/filter/vocab.hh                 — noncopyable + iterator_range + unordered_*
lm/filter/vocab.cc                 — unordered_*
lm/filter/thread.hh                — noncopyable + in_place_factory
lm/filter/phrase.hh                — unordered_map
lm/filter/phrase_table_vocab_main.cc — unordered_*
lm/builder/corpus_count.cc         — scoped_array
lm/builder/debug_print.hh          — lexical_cast
lm/builder/dump_counts_main.cc     — lexical_cast
lm/common/model_buffer.cc          — lexical_cast
util/integer_to_string.cc          — lexical_cast (check usage)
util/integer_to_string_test.cc     — lexical_cast
util/string_stream_test.cc         — lexical_cast
util/read_compressed_test.cc       — scoped_ptr
util/probing_hash_table_test.cc    — scoped_array + hash
util/sorted_uniform_test.cc        — scoped_array + unordered_map + random
```

### Modified files — CI
```
.github/workflows/*.yml            — remove boost install steps
```

---

### Task 1: Scaffold — Vendor dependencies, util::Range, C++17 bump

**Files:**
- Create: `third_party/CLI11/include/CLI/CLI.hpp`
- Create: `third_party/doctest/include/doctest/doctest.h`
- Create: `util/range.hh`
- Modify: `CMakeLists.txt` — C++17 bump only

**Interfaces:**
- Produces: `util::Range<I>` — template with `begin()`, `end()`, `empty()`, `size()`, `front()`, `back()`, `advance_begin(n)`, public `begin_`/`end_` members
- Produces: `third_party/CLI11/include/CLI/CLI.hpp` available for `#include <CLI/CLI.hpp>`
- Produces: `third_party/doctest/include/doctest/doctest.h` available for `#include <doctest/doctest.h>`

- [ ] **Step 1: Download CLI11 v2.4.2 single header**

```bash
curl -L https://github.com/CLIUtils/CLI11/releases/download/v2.4.2/CLI11.hpp -o third_party/CLI11/include/CLI/CLI11.hpp
# Or manually download from the release page
mkdir -p third_party/CLI11/include/CLI
```

- [ ] **Step 2: Download doctest v2.4.11 single header**

```bash
curl -L https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h -o third_party/doctest/include/doctest/doctest.h
mkdir -p third_party/doctest/include/doctest
```

- [ ] **Step 3: Create `util/range.hh`**

```cpp
#pragma once

#include <cstddef>

namespace util {

template <class I>
struct Range {
  I begin_;
  I end_;

  Range() : begin_(), end_() {}
  Range(I b, I e) : begin_(b), end_(e) {}

  I begin() const { return begin_; }
  I end() const { return end_; }
  bool empty() const { return begin_ == end_; }
  std::size_t size() const { return end_ - begin_; }

  typename std::iterator_traits<I>::reference front() const { return *begin_; }
  typename std::iterator_traits<I>::reference back() const { return *(end_ - 1); }

  void advance_begin(std::size_t n) { begin_ += n; }
};

} // namespace util
```

**Why `std::iterator_traits<I>::reference`:** Works for both raw pointers (`Width*`) and `std::vector::const_iterator`. Required because `vocab.hh` uses `const unsigned int*` and `multi_intersection.hh` uses generic iterators.

- [ ] **Step 4: Bump C++ standard in root CMakeLists.txt**

Edit `CMakeLists.txt`, change line 20:

```cmake
set(CMAKE_CXX_STANDARD 17)
```

Old text: `set(CMAKE_CXX_STANDARD 11)`

- [ ] **Step 5: Verify C++17 compiles**

```bash
cd D:/kenlm-forks && cmake -B build -DCMAKE_CXX_STANDARD=17 2>&1 | tail -5
```

Expected: Configures with no errors. May fail on `find_package(Boost)` — that's fine, we haven't removed it yet.

- [ ] **Step 6: Commit**

```bash
git add third_party/ util/range.hh CMakeLists.txt
git commit -m "scaffold: vendor CLI11 v2.4.2 + doctest v2.4.11, util::Range, C++17 bump"
```

---

### Task 2: Root CMakeLists.txt — Remove all boost references

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `cmake/kenlmConfig.cmake.in`

**Interfaces:**
- Consumes: C++17 bump from Task 1
- Produces: No more `find_package(Boost)`, no WIN32 BOOST_* defines, no `CMP0167` policy, no `Boost_INCLUDE_DIRS`, no `find_dependency(Boost)` in config
- Produces: `target_include_directories` for third_party headers

- [ ] **Step 1: Read the current root CMakeLists.txt to confirm exact lines**

```bash
cd D:/kenlm-forks && cat -n CMakeLists.txt
```

- [ ] **Step 2: Remove CMP0167 policy block (lines 2-8)**

Remove:
```cmake
# CMake >= 4.0 removed the FindBoost module. CMP0167=OLD restores it.
# On CI runners (≤3.31) this is a no-op; on 4.x dev machines it keeps builds working.
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()
```

- [ ] **Step 3: Remove WIN32 BOOST_* defines block (lines 9-17)**

Remove:
```cmake
if (WIN32)
    set(Boost_USE_STATIC_LIBS OFF)
    # The auto-linking feature has problems with USE_STATIC_LIBS off, so we use
    # BOOST_ALL_NO_LIB to turn it off.
    # Several boost libraries headers aren't configured correctly if
    # USE_STATIC_LIBS is off, so we explicitly say they are dynamic with the
    # remaining definitions.
    add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_PROGRAM_OPTIONS_DYN_LINK -DBOOST_IOSTREAMS_DYN_LINK -DBOOST_THREAD_DYN_LINK)
endif( )
```

- [ ] **Step 4: Remove `Boost_USE_STATIC_LIBS ON` in FORCE_STATIC block (line 51)**

Change line 51 from:
```cmake
  set(Boost_USE_STATIC_LIBS ON)
```
to: (delete the line entirely)

- [ ] **Step 5: Remove `find_package(Boost ...)` block and `include_directories(Boost_INCLUDE_DIRS)` (lines 108-121)**

Remove:
```cmake
# We need boost
# CMake 4.x removed FindBoost module — CMP0167=OLD restores it.
# Only program_options remains as a compiled library dependency.
# All other boost usage (thread, ptr_container, mutex, interprocess_semaphore,
# scoped_array, optional, reference_wrapper, lexical_cast, thread_specific_ptr)
# replaced with C++11 std equivalents.
# Modern Boost (Homebrew/vcpkg) uses Config mode; try Config first, module fallback.
find_package(Boost 1.90.0 QUIET CONFIG COMPONENTS program_options)
if(NOT Boost_FOUND)
  find_package(Boost 1.90.0 REQUIRED MODULE COMPONENTS program_options)
endif()

# Define where include files live
include_directories(${Boost_INCLUDE_DIRS})
```

- [ ] **Step 6: Add third_party include paths before `add_subdirectory(util)`**

After `set(THREADS_PREFER_PTHREAD_FLAG ON)` / `find_package(Threads REQUIRED)`:

```cmake
# Vendored third-party headers
add_library(kenlm_cli11 INTERFACE)
target_include_directories(kenlm_cli11 INTERFACE ${CMAKE_SOURCE_DIR}/third_party/CLI11/include)
add_library(kenlm_doctest INTERFACE)
target_include_directories(kenlm_doctest INTERFACE ${CMAKE_SOURCE_DIR}/third_party/doctest/include)
```

- [ ] **Step 7: Remove `find_dependency(Boost)` from `cmake/kenlmConfig.cmake.in`**

Change:
```cmake
find_dependency(Boost)
find_dependency(Threads)
```
To:
```cmake
find_dependency(Threads)
```

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt cmake/kenlmConfig.cmake.in
git commit -m "build: remove all boost from root CMakeLists.txt and config"
```

---

### Task 3: CMake — KenLMFunctions.cmake + subdirectory cleanup

**Files:**
- Modify: `cmake/KenLMFunctions.cmake`
- Modify: `util/CMakeLists.txt`
- Modify: `util/stream/CMakeLists.txt`
- Modify: `lm/builder/CMakeLists.txt`

**Interfaces:**
- Consumes: Root CMake changes from Task 2 (no more Boost_* vars)
- Produces: Tests link without boost, test lists renamed, Boost_LIBRARIES refs removed

- [ ] **Step 1: Remove BOOST_TEST_DYN_LINK logic from KenLMFunctions.cmake**

In `KenLMAddTest` function, remove lines 42-45:
```cmake
  if (Boost_USE_STATIC_LIBS)
    set(DYNLINK_FLAGS)
  else()
    set(DYNLINK_FLAGS COMPILE_FLAGS -DBOOST_TEST_DYN_LINK)
  endif()
```

And in `set_target_properties`, remove `${DYNLINK_FLAGS}` from:
```cmake
  set_target_properties(${KenLMAddTest_TEST} PROPERTIES
                        ${DYNLINK_FLAGS}
                        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)
```

Change to:
```cmake
  set_target_properties(${KenLMAddTest_TEST} PROPERTIES
                        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)
```

- [ ] **Step 2: Add doctest include to test targets in KenLMFunctions.cmake**

After the `target_link_libraries` call in `KenLMAddTest`, add:
```cmake
  target_link_libraries(${KenLMAddTest_TEST} kenlm_doctest)
```

- [ ] **Step 3: Update comment in KenLMFunctions.cmake**

Change line 73-74 from:
```cmake
  # Iterate through the Boost tests list
  foreach(test ${AddTests_TESTS})
```
To:
```cmake
  # Iterate through the tests list
  foreach(test ${AddTests_TESTS})
```

- [ ] **Step 4: Fix `util/CMakeLists.txt` — remove Boost_LIBRARIES**

Remove lines 87-88:
```cmake
  # Boost is required for building binaries and tests
  "$<BUILD_INTERFACE:${Boost_LIBRARIES}>"
```

The `target_link_libraries(kenlm_util ...)` call should remain but without those lines. Result:
```cmake
target_link_libraries(kenlm_util
  PRIVATE
  Threads::Threads
  ${RT})
```

- [ ] **Step 5: Rename test list variable in `util/CMakeLists.txt`**

Change line 109:
```cmake
  set(KENLM_BOOST_TESTS_LIST
```
To:
```cmake
  set(KENLM_TESTS_LIST
```

And line 123:
```cmake
  AddTests(TESTS ${KENLM_BOOST_TESTS_LIST}
```
To:
```cmake
  AddTests(TESTS ${KENLM_TESTS_LIST}
```

- [ ] **Step 6: Fix `util/stream/CMakeLists.txt` — rename test list + remove Boost_LIBRARIES**

Change line 29-30:
```cmake
  # Explicitly list the Boost test files to be compiled
  set(KENLM_BOOST_TESTS_LIST
```
To:
```cmake
  # Explicitly list the test files to be compiled
  set(KENLM_TESTS_LIST
```

Change line 37-38:
```cmake
  AddTests(TESTS ${KENLM_BOOST_TESTS_LIST}
           LIBRARIES kenlm_util ${Boost_LIBRARIES} Threads::Threads)
```
To:
```cmake
  AddTests(TESTS ${KENLM_TESTS_LIST}
           LIBRARIES kenlm_util Threads::Threads)
```

- [ ] **Step 7: Fix `lm/builder/CMakeLists.txt` — rename test list**

Change lines 51-52:
```cmake
  # Explicitly list the Boost test files to be compiled
  set(KENLM_BOOST_TESTS_LIST
```
To:
```cmake
  # Explicitly list the test files to be compiled
  set(KENLM_TESTS_LIST
```

Change line 57:
```cmake
  AddTests(TESTS ${KENLM_BOOST_TESTS_LIST}
```
To:
```cmake
  AddTests(TESTS ${KENLM_TESTS_LIST}
```

- [ ] **Step 8: Check `lm/CMakeLists.txt` for BOOST references**

```bash
grep -n 'BOOST\|boost\|Boost' lm/CMakeLists.txt
```

Expected: finds `KENLM_BOOST_TESTS_LIST` reference. Rename to `KENLM_TESTS_LIST`.

- [ ] **Step 9: Commit**

```bash
git add cmake/KenLMFunctions.cmake util/CMakeLists.txt util/stream/CMakeLists.txt lm/builder/CMakeLists.txt lm/CMakeLists.txt
git commit -m "build: remove boost from test infrastructure, rename test list vars"
```

---

### Task 4: CLI11 migration — size_option helper

**Files:**
- Modify: `lm/common/size_option.hh`
- Modify: `lm/common/size_option.cc`

**Interfaces:**
- Consumes: CLI11 header available from Task 1
- Produces: `lm::SizeOption(std::size_t &to, const char *default_value)` → returns `CLI::Option*` instead of `boost::program_options::typed_value<std::string>*`
- Produces: `size_option.cc` provides `SizeOption` function that creates CLI11 transform-based option

**Note:** `SizeOption` is used in `count_ngrams_main.cc`, `lmplz_main.cc`, and `interpolate_main.cc`. The return type changes so callers change too — but those callers are being migrated in subsequent tasks anyway. This task must be done before Task 5, 6, or 8.

- [ ] **Step 1: Rewrite `lm/common/size_option.hh`**

```cpp
#ifndef LM_COMMON_SIZE_OPTION_H
#define LM_COMMON_SIZE_OPTION_H

#include <CLI/CLI.hpp>

#include <cstddef>

namespace lm {

// Create a CLI11 option for data sizes.  This parses sizes like 1T and 10k.
CLI::Option *SizeOption(std::size_t &to, const char *default_value);

} // namespace lm

#endif // LM_COMMON_SIZE_OPTION_H
```

CLI11 v2.4.2 has no direct `typed_value`/`notifier` pattern matching `boost::program_options`. The cleanest approach: **delete `size_option.hh` and `size_option.cc` entirely.** Callers will add size options as plain strings, then call `util::ParseSize()` after CLI11 parses. `util::ParseSize` already exists in `util/usage.hh`.

Before (program_options pattern):
```cpp
#include "../common/size_option.hh"
options.add_options()("memory,S", lm::SizeOption(ram, "80%"), "RAM");
```

After (CLI11 pattern, in Tasks 5/6/8):
```cpp
std::string ram_str;
app.add_option("-S,--memory", ram_str, "RAM")->default_val("80%");
// ...after app.parse()...
size_t ram = util::ParseSize(ram_str);
```

- [ ] **Step 1: Delete `lm/common/size_option.hh` and `lm/common/size_option.cc`**

```bash
git rm lm/common/size_option.hh lm/common/size_option.cc
```

Verify nothing else includes `size_option.hh` directly (the callers do):
```bash
grep -rn 'size_option' --include='*.cc' --include='*.hh'
```

Expected: `lmplz_main.cc`, `count_ngrams_main.cc`, `interpolate_main.cc` include it. These are being migrated in Tasks 5-8.

- [ ] **Step 2: Commit**

```bash
git add lm/common/size_option.hh lm/common/size_option.cc
git commit -m "refactor: delete size_option helper — replaced by inline util::ParseSize"
```

---

### Task 5: CLI11 migration — count_ngrams_main.cc

**Files:**
- Modify: `lm/builder/count_ngrams_main.cc`

**Interfaces:**
- Consumes: CLI11 header (Task 1), util::ParseSize (Task 4 inline)
- Produces: `count_ngrams` binary uses CLI11 for arg parsing

- [ ] **Step 1: Rewrite `lm/builder/count_ngrams_main.cc`**

Replace the entire file:

```cpp
#include "combine_counts.hh"
#include "corpus_count.hh"
#include "../common/compare.hh"
#include "../../util/stream/chain.hh"
#include "../../util/stream/io.hh"
#include "../../util/stream/sort.hh"
#include "../../util/file.hh"
#include "../../util/file_piece.hh"
#include "../../util/usage.hh"

#include <CLI/CLI.hpp>

#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  unsigned order;
  std::string ram_str;
  std::string temp_prefix, vocab_table, vocab_list;

  CLI::App app{"corpus count"};
  bool help = false;
  app.add_flag("-h,--help", help, "Show this help message");
  app.add_option("-o,--order", order, "Order")->required();
  app.add_option("-T,--temp_prefix", temp_prefix, "Temporary file prefix")->default_val(util::DefaultTempDirectory());
  app.add_option("-S,--memory", ram_str, "RAM")->default_val("80%");
  app.add_option("--read_vocab_table", vocab_table, "Vocabulary hash table to read.  This should be a probing hash table with size at the beginning.");
  app.add_option("--write_vocab_list", vocab_list, "Vocabulary list to write as null-delimited strings.");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    return app.exit(e);
  }

  if (help) {
    std::cerr << "Counts n-grams from standard input.\n" << app.help() << std::endl;
    return 1;
  }

  std::size_t ram = util::ParseSize(ram_str);

  if (!(vocab_table.empty() ^ vocab_list.empty())) {
    std::cerr << "Specify one of --read_vocab_table or --write_vocab_list for vocabulary handling." << std::endl;
    return 1;
  }

  util::NormalizeTempPrefix(temp_prefix);

  util::scoped_fd vocab_file(vocab_table.empty() ? util::CreateOrThrow(vocab_list.c_str()) : util::OpenReadOrThrow(vocab_table.c_str()));

  std::size_t blocks = 2;
  std::size_t remaining_size = ram - util::SizeOrThrow(vocab_file.get());

  std::size_t memory_for_chain =
    static_cast<float>(remaining_size) /
    (static_cast<float>(blocks) + lm::builder::CorpusCount::DedupeMultiplier(order)) *
    static_cast<float>(blocks);
  std::cerr << "Using " << memory_for_chain << " for chains." << std::endl;
  
  util::stream::Chain chain(util::stream::ChainConfig(lm::NGram<uint64_t>::TotalSize(order), blocks, memory_for_chain));
  util::FilePiece f(0, NULL, &std::cerr);
  uint64_t token_count = 0;
  lm::WordIndex type_count = 0;
  std::vector<bool> empty_prune;
  std::string empty_string;
  lm::builder::CorpusCount counter(f, vocab_file.get(), vocab_table.empty(), token_count, type_count, empty_prune, empty_string, chain.BlockSize() / chain.EntrySize(), lm::THROW_UP);
  chain >> boost::ref(counter);

  util::stream::SortConfig sort_config;
  sort_config.temp_prefix = temp_prefix;
  sort_config.buffer_size = 64 * 1024 * 1024;
  sort_config.total_memory = remaining_size;
  util::stream::Sort<lm::SuffixOrder, lm::builder::CombineCounts> sorted(chain, sort_config, lm::SuffixOrder(order), lm::builder::CombineCounts());
  chain.Wait(true);
  util::stream::Chain chain2(util::stream::ChainConfig(lm::NGram<uint64_t>::TotalSize(order), blocks, sort_config.buffer_size));
  sorted.Output(chain2);
  chain2 >> util::stream::WriteAndRecycle(1);
  chain2.Wait(true);
  return 0;
}
```

Note: `boost::ref(counter)` on line `chain >> boost::ref(counter)` stays — this is `util::stream::Chain` operator usage, not boost::program_options. We'll handle the remaining `boost::ref` usages in a later task. Actually, `boost::ref` is used in the chain infrastructure (`util/stream/chain.hh`) — let me check if that's a separate boost usage.

```bash
grep -rn 'boost::ref\|boost/ref' --include='*.hh' --include='*.cc'
```

If found, we need to handle it. But for now, keep `boost::ref` — it's a separate concern from the CLI migration.

- [ ] **Step 2: Commit**

```bash
git add lm/builder/count_ngrams_main.cc
git commit -m "refactor: count_ngrams — boost::program_options → CLI11"
```

---

### Task 6: CLI11 migration — lmplz_main.cc (most complex CLI migration)

**Files:**
- Modify: `lm/builder/lmplz_main.cc`

**Interfaces:**
- Consumes: CLI11 header (Task 1), util::ParseSize inline (Task 4)
- Produces: `lmplz` binary uses CLI11; BOOST_VERSION ifdefs removed; lexical_cast → std

**Notes:** This file has:
1. `boost::program_options` — CLI arg parsing
2. `boost/version.hpp` — `BOOST_VERSION` ifdefs (remove, keep >= 104200 path)
3. `boost::lexical_cast` — in `ParsePruning` and `ParseDiscountFallback`
4. `boost::bad_lexical_cast` — in catch

- [ ] **Step 1: Rewrite includes and ParsePruning**

Replace includes (lines 1-13):

```cpp
#include "output.hh"
#include "pipeline.hh"
#include "../lm_exception.hh"
#include "../../util/file.hh"
#include "../../util/file_piece.hh"
#include "../../util/usage.hh"

#include <CLI/CLI.hpp>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>
```

- [ ] **Step 2: Rewrite ParsePruning (lines 17-47)**

Replace `boost::lexical_cast<uint64_t>` and `boost::bad_lexical_cast`:

```cpp
std::vector<uint64_t> ParsePruning(const std::vector<std::string> &param, std::size_t order) {
  std::vector<uint64_t> prune_thresholds;
  prune_thresholds.reserve(order);
  for (std::vector<std::string>::const_iterator it(param.begin()); it != param.end(); ++it) {
    try {
      std::size_t pos;
      uint64_t val = std::stoull(*it, &pos);
      if (pos != it->size()) throw std::invalid_argument("trailing characters");
      prune_thresholds.push_back(val);
    } catch(const std::exception &) {
      UTIL_THROW(util::Exception, "Bad pruning threshold " << *it);
    }
  }

  if (prune_thresholds.empty()) {
    prune_thresholds.resize(order, 0);
    return prune_thresholds;
  }

  UTIL_THROW_IF(prune_thresholds.size() > order, util::Exception, "You specified pruning thresholds for orders 1 through " << prune_thresholds.size() << " but the model only has order " << order);

  uint64_t lower_threshold = 0;
  for (std::vector<uint64_t>::iterator it = prune_thresholds.begin(); it != prune_thresholds.end(); ++it) {
    UTIL_THROW_IF(lower_threshold > *it, util::Exception, "Pruning thresholds should be in non-decreasing order.  Otherwise substrings would be removed, which is bad for query-time data structures.");
    lower_threshold = *it;
  }

  prune_thresholds.resize(order, prune_thresholds.back());
  return prune_thresholds;
}
```

- [ ] **Step 3: Rewrite ParseDiscountFallback**

Replace `boost::lexical_cast<float>` with `std::stof`:

```cpp
lm::builder::Discount ParseDiscountFallback(const std::vector<std::string> &param) {
  lm::builder::Discount ret;
  UTIL_THROW_IF(param.size() > 3, util::Exception, "Specify at most three fallback discounts: 1, 2, and 3+");
  UTIL_THROW_IF(param.empty(), util::Exception, "Fallback discounting enabled, but no discount specified");
  ret.amount[0] = 0.0;
  for (unsigned i = 0; i < 3; ++i) {
    float discount = std::stof(param[i < param.size() ? i : (param.size() - 1)]);
    UTIL_THROW_IF(discount < 0.0 || discount > static_cast<float>(i+1), util::Exception, "The discount for count " << (i+1) << " was parsed as " << discount << " which is not in the range [0, " << (i+1) << "].");
    ret.amount[i + 1] = discount;
  }
  return ret;
}
```

- [ ] **Step 4: Rewrite main() CLI section**

Replace the entire `main()` function with CLI11 version. Key mappings:

```cpp
int main(int argc, char *argv[]) {
  try {
    lm::builder::PipelineConfig pipeline;

    std::string text, intermediate, arpa;
    std::vector<std::string> pruning;
    std::vector<std::string> discount_fallback;
    std::string ram_str, minimum_block_str, sort_block_str;
    bool verbose_header;
    bool help = false;
    bool skip_symbols = false;
    bool renumber = false;
    bool collapse_values = false;

    CLI::App app{"Language model building options"};

    app.add_flag("-h,--help", help, "Show this help message");
    app.add_option("-o,--order", pipeline.order, "Order of the model")->required();
    app.add_option("--interpolate_unigrams", pipeline.initial_probs.interpolate_unigrams,
      "Interpolate the unigrams (default) as opposed to giving lots of mass to <unk> like SRI.  If you want SRI's behavior with a large <unk> and the old lmplz default, use --interpolate_unigrams 0.")
      ->default_val(true);
    app.add_flag("--skip_symbols", skip_symbols,
      "Treat <s>, </s>, and <unk> as whitespace instead of throwing an exception");
    app.add_option("-T,--temp_prefix", pipeline.sort.temp_prefix, "Temporary file prefix")
      ->default_val(util::DefaultTempDirectory());
    app.add_option("-S,--memory", ram_str, "Sorting memory")
      ->default_val(util::GuessPhysicalMemory() ? "80%" : "1G");
    app.add_option("--minimum_block", minimum_block_str, "Minimum block size to allow")
      ->default_val("8K");
    app.add_option("--sort_block", sort_block_str, "Size of IO operations for sort (determines arity)")
      ->default_val("64M");
    app.add_option("--block_count", pipeline.block_count, "Block count (per order)")->default_val(2);
    app.add_option("--vocab_estimate", pipeline.vocab_estimate,
      "Assume this vocabulary size for purposes of calculating memory in step 1 (corpus count) and pre-sizing the hash table")
      ->default_val(1000000);
    app.add_option("--vocab_pad", pipeline.vocab_size_for_unk,
      "If the vocabulary is smaller than this value, pad with <unk> to reach this size. Requires --interpolate_unigrams")
      ->default_val(0);
    app.add_flag("--verbose_header", verbose_header,
      "Add a verbose header to the ARPA file that includes information such as token count, smoothing type, etc.");
    app.add_option("--text", text, "Read text from a file instead of stdin");
    app.add_option("--arpa", arpa, "Write ARPA to a file instead of stdout");
    app.add_option("--intermediate", intermediate,
      "Write ngrams to intermediate files.  Turns off ARPA output (which can be reactivated by --arpa file).  Forces --renumber on.");
    app.add_flag("--renumber", renumber,
      "Renumber the vocabulary identifiers so that they are monotone with the hash of each string.  This is consistent with the ordering used by the trie data structure.");
    app.add_flag("--collapse_values", collapse_values,
      "Collapse probability and backoff into a single value, q that yields the same sentence-level probabilities.  See http://kheafield.com/professional/edinburgh/rest_paper.pdf for more details, including a proof.");
    app.add_option("--prune", pruning,
      "Prune n-grams with count less than or equal to the given threshold.  Specify one value for each order i.e. 0 0 1 to prune singleton trigrams and above.  The sequence of values must be non-decreasing and the last value applies to any remaining orders. Default is to not prune, which is equivalent to --prune 0.")
      ->expected(-1);
    app.add_option("--limit_vocab_file", pipeline.prune_vocab_file,
      "Read allowed vocabulary separated by whitespace. N-grams that contain vocabulary items not in this list will be pruned. Can be combined with --prune arg")
      ->default_val("");
    app.add_option("--discount_fallback", discount_fallback,
      "The closed-form estimate for Kneser-Ney discounts does not work without singletons or doubletons.  It can also fail if these values are out of range.  This option falls back to user-specified discounts when the closed-form estimate fails.  Note that this option is generally a bad idea: you should deduplicate your corpus instead.  However, class-based models need custom discounts because they lack singleton unigrams.  Provide up to three discounts (for adjusted counts 1, 2, and 3+), which will be applied to all orders where the closed-form estimates fail.")
      ->expected(-1);

    try {
      app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
      return app.exit(e);
    }

    if (help) {
      std::cerr <<
        "Builds unpruned language models with modified Kneser-Ney smoothing.\n\n"
        "Please cite:\n"
        "@inproceedings{Heafield-estimate,\n"
        "  author = {Kenneth Heafield and Ivan Pouzyrevsky and Jonathan H. Clark and Philipp Koehn},\n"
        "  title = {Scalable Modified {Kneser-Ney} Language Model Estimation},\n"
        "  year = {2013},\n"
        "  month = {8},\n"
        "  booktitle = {Proceedings of the 51st Annual Meeting of the Association for Computational Linguistics},\n"
        "  address = {Sofia, Bulgaria},\n"
        "  url = {http://kheafield.com/professional/edinburgh/estimate\\_paper.pdf},\n"
        "}\n\n"
        "Provide the corpus on stdin.  The ARPA file will be written to stdout.  Order of\n"
        "the model (-o) is the only mandatory option.  As this is an on-disk program,\n"
        "setting the temporary file location (-T) and sorting memory (-S) is recommended.\n\n"
        "Memory sizes are specified like GNU sort: a number followed by a unit character.\n"
        "Valid units are \% for percentage of memory (supported platforms only) and (in\n"
        "increasing powers of 1024): b, K, M, G, T, P, E, Z, Y.  Default is K (*1024).\n";
      uint64_t mem = util::GuessPhysicalMemory();
      if (mem) {
        std::cerr << "This machine has " << mem << " bytes of memory.\n\n";
      } else {
        std::cerr << "Unable to determine the amount of memory on this machine.\n\n";
      }
      std::cerr << app.help() << std::endl;
      return 1;
    }

    // Parse size strings AFTER help check
    pipeline.sort.total_memory = util::ParseSize(ram_str);
    pipeline.minimum_block = util::ParseSize(minimum_block_str);
    pipeline.sort.buffer_size = util::ParseSize(sort_block_str);

    pipeline.renumber_vocabulary = renumber;
    pipeline.output_q = collapse_values;
    if (skip_symbols) {
      pipeline.disallowed_symbol_action = lm::COMPLAIN;
    } else {
      pipeline.disallowed_symbol_action = lm::THROW_UP;
    }
```

**Rest of main unchanged** from the `if (pipeline.vocab_size_for_unk && ...)` line onward. Just replace `vm["skip_symbols"].as<bool>()` with `skip_symbols`, and all `vm["..."]` references with the corresponding variable.

**Crucially**, the `#if BOOST_VERSION < 104200` block (lines 144-149 in original) is simply removed — CLI11's `required()` always works.

- [ ] **Step 2: Commit**

```bash
git add lm/builder/lmplz_main.cc
git commit -m "refactor: lmplz — boost::program_options → CLI11, lexical_cast → stoull/stof, remove BOOST_VERSION"
```

---

### Task 7: CLI11 migration — kenlm_benchmark_main.cc (program_options + iterator_range)

**Files:**
- Modify: `lm/kenlm_benchmark_main.cc`

**Interfaces:**
- Consumes: CLI11 header (Task 1), `util/range.hh` (Task 1)
- Produces: `kenlm_benchmark` binary uses CLI11; `boost::iterator_range<Width*>` → `util::Range<Width*>`

- [ ] **Step 1: Rewrite includes and main() CLI section**

Replace:
```cpp
#include <boost/range/iterator_range.hpp>
#include <boost/program_options.hpp>
```
With:
```cpp
#include <CLI/CLI.hpp>
#include "../../util/range.hh"
```

- [ ] **Step 2: Replace iterator_range usage throughout**

Every `boost::iterator_range<Width *>` → `util::Range<Width *>`.

Construction: `boost::iterator_range<Width *>(a, b)` → `util::Range<Width *>{a, b}`.

Null range: `boost::iterator_range<Width *>((Width*)0, (Width*)0)` → `util::Range<Width *>()`.

- [ ] **Step 3: Rewrite main() CLI section**

```cpp
int main(int argc, char *argv[]) {
  try {
    Config config;
    config.fd_in = 0;
    std::string model;

    CLI::App app{"Benchmark options"};
    bool help = false;
    bool vocab_mode = false;
    bool query_mode = false;
    app.add_flag("-h,--help", help, "Show help message");
    app.add_option("-m,--model", model, "Model to query or convert vocab ids")->required();
    app.add_option("-t,--threads", config.threads, "Threads to use (querying only; TODO vocab conversion)")
      ->default_val(std::thread::hardware_concurrency());
    app.add_option("-b,--buffer", config.buf_per_thread, "Number of words to buffer per task.")->default_val(4096);
    app.add_flag("-v,--vocab", vocab_mode, "Convert strings to vocab ids");
    app.add_flag("-q,--query", query_mode, "Query from vocab ids");

    try {
      app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
      return app.exit(e);
    }

    if (help) {
      std::cerr << "Benchmark program for KenLM.  Intended usage:\n"
        << "#Convert text to vocabulary ids offline.  These ids are tied to a model.\n"
        << argv[0] << " -v -m $model <$text >$text.vocab\n"
        << "#Ensure files are in RAM.\n"
        << "cat $text.vocab $model >/dev/null\n"
        << "#Timed query against the model.\n"
        << argv[0] << " -q -m $model <$text.vocab\n";
      return 0;
    }

    if (!(vocab_mode ^ query_mode)) {
      std::cerr << "Specify exactly one of -v (vocab conversion) or -q (query)." << std::endl;
      return 0;
    }
    config.query = query_mode;
    if (!config.threads) {
      std::cerr << "Specify a non-zero number of threads with -t." << std::endl;
    }
    Dispatch(model.c_str(), config);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
```

- [ ] **Step 4: Commit**

```bash
git add lm/kenlm_benchmark_main.cc
git commit -m "refactor: kenlm_benchmark — CLI11 + util::Range replacing boost::iterator_range"
```

---

### Task 8: CLI11 migration — interpolate_main.cc, streaming_example_main.cc, tune_weights.cc

**Files:**
- Modify: `lm/interpolate/interpolate_main.cc`
- Modify: `lm/interpolate/streaming_example_main.cc`
- Modify: `lm/interpolate/tune_weights.cc`

**Interfaces:**
- Consumes: CLI11 header (Task 1), util::ParseSize (Task 4)

- [ ] **Step 1: Rewrite `lm/interpolate/interpolate_main.cc`**

This file has `MungeWeightArgs` hack for negative weight values like `-w 0.2 -0.1`. CLI11 handles negative numbers natively — no hack needed. The `--weight,w` option becomes a simple vector.

Key changes:
- Remove `MungeWeightArgs` entirely
- Remove `#include <boost/program_options.hpp>`, add `#include <CLI/CLI.hpp>`
- Replace `size_option.hh` include with direct `util/usage.hh` for `ParseSize`
- `--model,m` → `app.add_option("-m,--model", input_models, "...")->required()->expected(-1)`
- `--weight,w` → `app.add_option("-w,--weight", pipe_config.lambdas, "...")->expected(-1)`
- `--tuning,t` → `app.add_option("-t,--tuning", tuning_file, "...")`
- `--just_tune` → `app.add_flag("--just_tune", just_tune, "...")`
- `--temp_prefix,T` → `app.add_option("-T,--temp_prefix", pipe_config.sort.temp_prefix, "...")->default_val("/tmp/lm")`
- `--memory,S` → `app.add_option("-S,--memory", ram_str, "...")->default_val(...)` then `pipe_config.sort.total_memory = util::ParseSize(ram_str)`
- `--sort_block` → `app.add_option("--sort_block", sort_block_str, "...")->default_val("64M")` then `pipe_config.sort.buffer_size = util::ParseSize(sort_block_str)`

- [ ] **Step 2: Rewrite `lm/interpolate/streaming_example_main.cc`**

Simple CLI11 migration:
- Replace `#include <boost/program_options.hpp>` and `#include <boost/version.hpp>` with `#include <CLI/CLI.hpp>`
- Convert options to CLI11 pattern

- [ ] **Step 3: Rewrite `lm/interpolate/tune_weights.cc`**

This file includes `boost/program_options.hpp` but doesn't actually use it. The actual `main()` or tuning logic is called from `TuneWeights()` which takes parameters directly. Just remove the include.

- [ ] **Step 4: Commit**

```bash
git add lm/interpolate/interpolate_main.cc lm/interpolate/streaming_example_main.cc lm/interpolate/tune_weights.cc
git commit -m "refactor: interpolate + streaming_example + tune_weights → CLI11"
```

---

### Task 9: doctest migration — util/ test files (batch 1, 10 files)

**Files:**
- Modify: `util/bit_packing_test.cc`
- Modify: `util/integer_to_string_test.cc`
- Modify: `util/joint_sort_test.cc`
- Modify: `util/multi_intersection_test.cc`
- Modify: `util/pcqueue_test.cc`
- Modify: `util/probing_hash_table_test.cc`
- Modify: `util/read_compressed_test.cc`
- Modify: `util/sized_iterator_test.cc`
- Modify: `util/sorted_uniform_test.cc`
- Modify: `util/string_stream_test.cc`
- Modify: `util/file_piece_test.cc`
- Modify: `util/tokenize_piece_test.cc`
- Modify: `util/stream/io_test.cc`
- Modify: `util/stream/sort_test.cc`
- Modify: `util/stream/stream_test.cc`
- Modify: `util/stream/rewindable_stream_test.cc`

**Interfaces:**
- Consumes: doctest header (Task 1), CMake test setup (Task 3)
- Produces: All util/ tests compile and pass with doctest

**Migration pattern for each file:**

1. Replace `#define BOOST_TEST_MODULE Name` + `#include <boost/test/included/unit_test.hpp>` with:
```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
```

2. Replace assertions:
- `BOOST_AUTO_TEST_CASE(Name)` → `TEST_CASE("Name")`
- `BOOST_CHECK_EQUAL(a, b)` → `CHECK_EQ(a, b)`
- `BOOST_REQUIRE_EQUAL(a, b)` → `REQUIRE_EQ(a, b)`
- `BOOST_CHECK(a)` → `CHECK(a)`
- `BOOST_CHECK(!a)` → `CHECK_FALSE(a)`
- `BOOST_REQUIRE(a)` → `REQUIRE(a)`
- `BOOST_CHECK_THROW(stmt, ex)` → `CHECK_THROWS_AS(stmt, ex)`

3. Remove `#include <boost/test/floating_point_comparison.hpp>` if present.

**Note on boost::scoped_array / boost::hash / boost::random in test files:** These are NOT test-framework usages — they're utility usages that happen to be in test files. Leave them for Tasks 12-16. For this task, only change the test framework headers + macros.

- [ ] **Step 1: Batch-migrate all util/ test files**

For each test file, apply the pattern:
```bash
# For each *_test.cc file in util/ and util/stream/:
# 1. Replace the BOOST_TEST_MODULE + include block
# 2. Replace BOOST_AUTO_TEST_CASE → TEST_CASE
# 3. Replace assertion macros
```

Show the exact before/after for one file as representative, note "do same for all others."

Example — `util/bit_packing_test.cc`:
```cpp
// Before
#define BOOST_TEST_MODULE BitPackingTest
#include <boost/test/included/unit_test.hpp>
...
BOOST_AUTO_TEST_CASE(ZeroBit) { ... }

// After
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
...
TEST_CASE("ZeroBit") { ... }
```

- [ ] **Step 2: Verify build with tests enabled**

```bash
cd D:/kenlm-forks && cmake -B build -DCOMPILE_TESTS=ON 2>&1 | tail -20
cmake --build build --target bit_packing_test 2>&1
```

Expected: test binary compiles. (May fail on boost::scoped_array etc in test bodies — that's fine, those get fixed in Tasks 12-16.)

- [ ] **Step 3: Commit**

```bash
git add util/*_test.cc util/stream/*_test.cc
git commit -m "test: migrate util tests — boost::test → doctest"
```

---

### Task 10: doctest migration — lm/ test files (batch 2, 10 files)

**Files:**
- Modify: `lm/left_test.cc`
- Modify: `lm/model_test.cc`
- Modify: `lm/partial_test.cc`
- Modify: `lm/builder/adjust_counts_test.cc`
- Modify: `lm/builder/corpus_count_test.cc`
- Modify: `lm/common/model_buffer_test.cc`
- Modify: `lm/interpolate/backoff_reunification_test.cc`
- Modify: `lm/interpolate/bounded_sequence_encoding_test.cc`
- Modify: `lm/interpolate/merge_vocab_test.cc`
- Modify: `lm/interpolate/normalize_test.cc`
- Modify: `lm/interpolate/tune_derivatives_test.cc`
- Modify: `lm/interpolate/tune_instances_test.cc`

**Interfaces:**
- Consumes: doctest header (Task 1), CMake test setup (Task 3)
- Produces: All lm/ tests compile and pass with doctest

- [ ] **Step 1: Migrate `lm/left_test.cc`**

This file additionally includes `boost/test/floating_point_comparison.hpp` and uses `BOOST_CHECK_CLOSE`. Replace:

```cpp
// Before
#include <boost/test/included/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
...
BOOST_CHECK_CLOSE(prob, ref_prob, 0.1);

// After
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
...
CHECK(prob == doctest::Approx(ref_prob).epsilon(0.001));
```

Same pattern for `lm/model_test.cc` and `lm/partial_test.cc`.

- [ ] **Step 2: Migrate all other lm/ test files**

Same macro replacement pattern as Task 9. `BOOST_CHECK_CLOSE` in files that use it → doctest `Approx`.

`corpus_count_test.cc` uses a custom macro `CHECK_STREAM_COUNT`. That macro uses `BOOST_REQUIRE`, `BOOST_CHECK_EQUAL`. Replace macro body:

```cpp
// Before
#define CHECK_STREAM_COUNT(stream, cnt, v, w, t) \
  BOOST_REQUIRE(stream); \
  ... BOOST_CHECK_EQUAL(...) ... BOOST_CHECK_EQUAL(...)

// After
#define CHECK_STREAM_COUNT(stream, cnt, v, w, t) \
  REQUIRE(stream); \
  ... CHECK_EQ(...) ... CHECK_EQ(...)
```

- [ ] **Step 3: Commit**

```bash
git add lm/*_test.cc lm/builder/*_test.cc lm/common/*_test.cc lm/interpolate/*_test.cc
git commit -m "test: migrate lm tests — boost::test → doctest"
```

---

### Task 11: Utility replacements — boost::optional, noncopyable, in_place_factory

**Files:**
- Modify: `util/multi_intersection.hh`
- Modify: `lm/interpolate/tune_instances.hh`
- Modify: `util/stream/stream.hh`
- Modify: `util/stream/rewindable_stream.hh`
- Modify: `lm/filter/arpa_io.hh`
- Modify: `lm/filter/vocab.hh`
- Modify: `lm/filter/thread.hh`

**Interfaces:**
- Consumes: `util/range.hh` (for multi_intersection), C++17 std::optional
- Produces: No more boost::optional, boost::noncopyable, boost::in_place_factory

- [ ] **Step 1: Rewrite `util/multi_intersection.hh`**

Replace:
```cpp
#include <boost/optional.hpp>
#include <boost/range/iterator_range.hpp>
```
With:
```cpp
#include <optional>
#include "util/range.hh"
```

Replace all `boost::optional<...>` → `std::optional<...>`.
Replace all `boost::iterator_range<Iterator>` → `util::Range<Iterator>`.
Replace `return boost::optional<Value>()` → `return std::nullopt`.
Replace `return boost::optional<Value>(v)` → `return v` (implicit conversion from T to optional<T>).
Update `detail::RangeLessBySize` to use `util::Range`.

- [ ] **Step 2: Rewrite `lm/interpolate/tune_instances.hh`**

Replace `#include <boost/optional.hpp>` → `#include <optional>`. Replace any `boost::optional` usage with `std::optional`.

- [ ] **Step 3: Rewrite noncopyable classes (5 files)**

For each class inheriting `boost::noncopyable`, add deleted copy members:

`util/stream/stream.hh`:
```cpp
// Before
class Stream : boost::noncopyable {

// After
class Stream {
  public:
    Stream() : current_(NULL), end_(NULL) {}
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
```

`util/stream/rewindable_stream.hh`:
```cpp
class RewindableStream {
  public:
    RewindableStream() {}
    RewindableStream(const RewindableStream&) = delete;
    RewindableStream& operator=(const RewindableStream&) = delete;
```

`lm/filter/arpa_io.hh`:
```cpp
class ARPAOutput {
  public:
    explicit ARPAOutput(const char *name, size_t buffer_size = 65536);
    ARPAOutput(const ARPAOutput&) = delete;
    ARPAOutput& operator=(const ARPAOutput&) = delete;
```

`lm/filter/vocab.hh`: Remove `#include <boost/noncopyable.hpp>`. Class `Single`, `Union`, `Multiple` don't inherit `boost::noncopyable` — they inherit nothing. Just remove the include.

`lm/filter/thread.hh`: Remove `boost::noncopyable` from `Controller` class. Add `= delete`.

- [ ] **Step 4: Rewrite `lm/filter/thread.hh` in_place_factory**

Line 109-110:
```cpp
// Before
output_(queue, 1, boost::in_place(boost::ref(output), boost::ref(to_read_)), NULL),
filter_(queue, workers, boost::in_place(boost::ref(filter), boost::ref(output_.In())), NULL),

// After
output_(queue, 1, std::ref(output), std::ref(to_read_)),
filter_(queue, workers, std::ref(filter), std::ref(output_.In())),
```

If the queue constructor takes variadic args and forwards them to the element constructor, then `std::ref(x), std::ref(y)` works as direct arguments. If it specifically expects an `in_place` tag, we need to check the queue implementation.

```bash
grep -n 'class.*Queue\|in_place\|template.*Queue' util/pcqueue.hh
```

Check what `ThreadQueue` / `PCQueue` constructor expects. If it's just forwarding args, direct construction works. If `in_place` is required, use a wrapper.

- [ ] **Step 5: Commit**

```bash
git add util/multi_intersection.hh lm/interpolate/tune_instances.hh \
        util/stream/stream.hh util/stream/rewindable_stream.hh \
        lm/filter/arpa_io.hh lm/filter/vocab.hh lm/filter/thread.hh
git commit -m "refactor: boost::optional → std::optional, noncopyable → =delete, in_place → direct"
```

---

### Task 12: Utility replacements — shared_ptr, scoped_ptr, scoped_array

**Files:**
- Modify: `lm/interpolate/tune_instances.cc`
- Modify: `util/read_compressed_test.cc`
- Modify: `lm/builder/corpus_count.cc`
- Modify: `util/probing_hash_table_test.cc`
- Modify: `util/sorted_uniform_test.cc`
- Modify: `lm/filter/arpa_io.hh`

**Interfaces:**
- Consumes: C++11 std::unique_ptr, std::shared_ptr
- Produces: No more boost::shared_ptr, scoped_ptr, scoped_array

- [ ] **Step 1: Replace boost::shared_ptr in tune_instances.cc**

Check exact usage:
```bash
grep -n 'shared_ptr\|make_shared' lm/interpolate/tune_instances.cc
```

Replace `#include <boost/shared_ptr.hpp>` with `#include <memory>`. Replace `boost::shared_ptr<T>` → `std::shared_ptr<T>`. Replace `boost::make_shared<T>(...)` → `std::make_shared<T>(...)`.

- [ ] **Step 2: Replace boost::scoped_ptr in read_compressed_test.cc**

```bash
grep -n 'scoped_ptr\|#include.*boost' util/read_compressed_test.cc
```

Replace `#include <boost/scoped_ptr.hpp>` with `#include <memory>`. Replace `boost::scoped_ptr<FILE>` → `std::unique_ptr<FILE>`. The `reset()` and `get()` methods work identically.

- [ ] **Step 3: Replace boost::scoped_array in corpus_count.cc**

```bash
grep -n 'scoped_array\|#include.*boost' lm/builder/corpus_count.cc
```

Replace `boost::scoped_array<WordIndex> buffer_` → `std::unique_ptr<WordIndex[]> buffer_`. The `reset(new T[n])` and `operator[]` work identically. Add `#include <memory>` if not present.

- [ ] **Step 4: Replace boost::scoped_array in probing_hash_table_test.cc**

Replace `#include <boost/scoped_array.hpp>` with `#include <memory>`. Replace `boost::scoped_array<T>` → `std::unique_ptr<T[]>`.

- [ ] **Step 5: Replace boost::scoped_array in sorted_uniform_test.cc**

Same pattern.

- [ ] **Step 6: Replace boost::scoped_array in lm/filter/arpa_io.hh**

Check exact usage:
```bash
grep -n 'scoped_array' lm/filter/arpa_io.hh
```

Replace `#include <boost/scoped_array.hpp>` with `#include <memory>`. Replace `boost::scoped_array<T>` → `std::unique_ptr<T[]>`.

- [ ] **Step 7: Commit**

```bash
git add lm/interpolate/tune_instances.cc util/read_compressed_test.cc \
        lm/builder/corpus_count.cc util/probing_hash_table_test.cc \
        util/sorted_uniform_test.cc lm/filter/arpa_io.hh
git commit -m "refactor: boost::shared_ptr/scoped_ptr/scoped_array → std equivalents"
```

---

### Task 13: Utility replacements — hash, random, lexical_cast, version.hpp

**Files:**
- Modify: `util/probing_hash_table_test.cc` (hash)
- Modify: `util/sorted_uniform_test.cc` (random)
- Modify: `lm/builder/dump_counts_main.cc` (lexical_cast)
- Modify: `lm/builder/debug_print.hh` (lexical_cast)
- Modify: `lm/common/model_buffer.cc` (lexical_cast)
- Modify: `util/integer_to_string.cc` (lexical_cast — check)
- Modify: `util/integer_to_string_test.cc` (lexical_cast)
- Modify: `util/string_stream_test.cc` (lexical_cast)
- Modify: `lm/builder/lmplz_main.cc` (version.hpp — already done in Task 6)
- Modify: `lm/interpolate/streaming_example_main.cc` (version.hpp — already done in Task 8)

- [ ] **Step 1: Replace boost::hash in probing_hash_table_test.cc**

```bash
grep -n 'boost::hash\|boost/functional' util/probing_hash_table_test.cc
```

Replace `#include <boost/functional/hash.hpp>` with `#include <functional>`. Replace `boost::hash<uint64_t>` → `std::hash<uint64_t>`.

- [ ] **Step 2: Replace boost::random in sorted_uniform_test.cc**

```cpp
// Before
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
...
boost::mt19937 gen;
boost::uniform_int<> dist(0, max_value);
boost::variate_generator<boost::mt19937&, boost::uniform_int<>> rng(gen, dist);
int val = rng();

// After
#include <random>
...
std::mt19937 gen;
std::uniform_int_distribution<int> dist(0, max_value);
int val = dist(gen);
```

- [ ] **Step 3: Replace lexical_cast in dump_counts_main.cc**

```bash
grep -n 'lexical_cast' lm/builder/dump_counts_main.cc
```

If `boost::lexical_cast<std::size_t>(str)` → `std::stoull(str)`.
If `boost::lexical_cast<double>(str)` → `std::stod(str)`.

- [ ] **Step 4: Replace lexical_cast in debug_print.hh**

```bash
grep -n 'lexical_cast' lm/builder/debug_print.hh
```

Replace `boost::lexical_cast<std::string>(val)` → `std::to_string(val)`.

- [ ] **Step 5: Replace lexical_cast in model_buffer.cc**

```bash
grep -n 'lexical_cast' lm/common/model_buffer.cc
```

Replace `boost::lexical_cast<double>(str)` → `std::stod(str)`.

- [ ] **Step 6: Replace lexical_cast in integer_to_string.cc**

```bash
grep -n 'lexical_cast\|boost' util/integer_to_string.cc
```

If present, replace accordingly.

- [ ] **Step 7: Replace lexical_cast in integer_to_string_test.cc**

```bash
grep -n 'lexical_cast' util/integer_to_string_test.cc
```

Replace `boost::lexical_cast<std::string>(val)` → `std::to_string(val)`.

- [ ] **Step 8: Replace lexical_cast in string_stream_test.cc**

```bash
grep -n 'lexical_cast' util/string_stream_test.cc
```

Replace `boost::lexical_cast<double>(str)` → `std::stod(str)`. Replace `boost::lexical_cast<std::string>(val)` → `std::to_string(val)`.

- [ ] **Step 9: Commit**

```bash
git add util/probing_hash_table_test.cc util/sorted_uniform_test.cc \
        lm/builder/dump_counts_main.cc lm/builder/debug_print.hh \
        lm/common/model_buffer.cc util/integer_to_string.cc \
        util/integer_to_string_test.cc util/string_stream_test.cc
git commit -m "refactor: boost::hash/random/lexical_cast → std equivalents"
```

---

### Task 14: Utility replacements — unordered_map / unordered_set across all remaining files

**Files:**
- Modify: `lm/filter/vocab.hh`
- Modify: `lm/filter/vocab.cc`
- Modify: `lm/filter/phrase.hh`
- Modify: `lm/filter/phrase_table_vocab_main.cc`
- Modify: `lm/interpolate/tune_instances.cc`

**Interfaces:**
- Consumes: C++11 std::unordered_map, std::unordered_set
- Produces: No more boost::unordered_* anywhere

- [ ] **Step 1: Replace in vocab.hh**

Replace:
```cpp
#include <boost/unordered/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>
```
With:
```cpp
#include <unordered_map>
#include <unordered_set>
```

Replace all `boost::unordered_set<std::string>` → `std::unordered_set<std::string>`.
Replace all `boost::unordered_map<std::string, std::vector<unsigned int>>` → `std::unordered_map<std::string, std::vector<unsigned int>>`.

- [ ] **Step 2: Replace in vocab.cc**

Replace `boost::unordered_*` → `std::unordered_*`. Add `#include <string>` and `#include <vector>` if the replacement `#include <unordered_map>` doesn't transitively bring them in.

- [ ] **Step 3: Replace in phrase.hh**

Replace `#include <boost/unordered_map.hpp>` → `#include <unordered_map>`. Replace `boost::unordered_map` usage.

- [ ] **Step 4: Replace in phrase_table_vocab_main.cc**

Replace includes: `<boost/unordered_map.hpp>` → `<unordered_map>`, `<boost/unordered_set.hpp>` → `<unordered_set>`. Replace type names.

- [ ] **Step 5: Replace in tune_instances.cc**

Replace `#include <boost/unordered_map.hpp>` → `#include <unordered_map>`. Replace type name.

- [ ] **Step 6: Commit**

```bash
git add lm/filter/vocab.hh lm/filter/vocab.cc lm/filter/phrase.hh \
        lm/filter/phrase_table_vocab_main.cc lm/interpolate/tune_instances.cc
git commit -m "refactor: boost::unordered_map/set → std::unordered_map/set"
```

---

### Task 15: Verify boost-free — final grep, build, tests

**Files:** All modified files from Tasks 1-14

- [ ] **Step 1: grep for remaining boost references**

```bash
cd D:/kenlm-forks
grep -rn '#include.*boost' --include='*.cc' --include='*.hh' --include='*.h' --include='*.hpp'
grep -rn 'boost::' --include='*.cc' --include='*.hh' --include='*.h' --include='*.hpp'
grep -rn 'Boost' CMakeLists.txt cmake/
```

Expected: No output. If `boost::ref` remains in chain infrastructure (`util/stream/chain.hh` uses `boost::ref`), note it — we handle that next.

- [ ] **Step 2: Handle remaining boost::ref if found**

```bash
grep -rn 'boost::ref\|boost/ref' --include='*.hh' --include='*.cc'
```

If found (likely in `util/stream/chain.hh` or callers), replace `boost::ref(x)` → `std::ref(x)`. Add `#include <functional>` if needed.

`boost::ref` is trivially `std::ref` — same semantics, C++11.

- [ ] **Step 3: CMake configure**

```bash
cd D:/kenlm-forks && cmake -B build -DCOMPILE_TESTS=ON 2>&1
```

Expected: Configures without errors. No "FindBoost" or "Boost" in output.

- [ ] **Step 4: Build all targets**

```bash
cmake --build build 2>&1 | tail -30
```

Expected: All targets compile. Fix any compilation errors inline.

- [ ] **Step 5: Run tests**

```bash
cd build && ctest --output-on-failure 2>&1
```

Expected: All tests pass.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "refactor: remove remaining boost::ref, verify boost-free build"
```

---

### Task 16: CI — Remove boost from GitHub Actions

**Files:**
- Modify: `.github/workflows/*.yml`

**Interfaces:**
- Consumes: Boost-free build from Task 15
- Produces: CI passes without boost installed

- [ ] **Step 1: Find CI workflow files**

```bash
find .github/workflows -name '*.yml' -exec grep -l 'boost\|Boost' {} \;
```

- [ ] **Step 2: Remove boost install steps from each workflow**

For each workflow, remove lines like:
```yaml
- name: Install Boost
  run: sudo apt-get install -y libboost-all-dev
```

Or:
```yaml
- uses: actions/cache@v3
  with:
    path: ~/boost
    key: ${{ runner.os }}-boost
```

- [ ] **Step 3: Verify CI would succeed locally**

```bash
# Simulate CI build
cd D:/kenlm-forks && cmake -B ci-build -DCOMPILE_TESTS=ON && cmake --build ci-build && cd ci-build && ctest
```

Expected: Builds + tests pass.

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/
git commit -m "ci: remove boost dependency from GitHub Actions"
```

---

### Task 17: Final regression test — lmplz produces identical ARPA

**Files:** None (verification only)

- [ ] **Step 1: Create test input**

```bash
echo "hello world" > /tmp/test_corpus.txt
echo "hello there" >> /tmp/test_corpus.txt
echo "world peace" >> /tmp/test_corpus.txt
```

- [ ] **Step 2: Run lmplz and capture ARPA output**

```bash
cd D:/kenlm-forks
./build/bin/lmplz -o 3 -S 10M < /tmp/test_corpus.txt > /tmp/test.arpa 2>/dev/null
```

- [ ] **Step 3: Verify ARPA has expected structure**

```bash
head -5 /tmp/test.arpa
```

Expected: Starts with `\data\`, followed by ngram counts, then `\1-grams:`, etc.

- [ ] **Step 4: Verify lmplz --help works**

```bash
./build/bin/lmplz --help 2>&1 | head -5
```

Expected: Shows help text with all CLI11 options.

- [ ] **Step 5: Verify count_ngrams --help, kenlm_benchmark --help, interpolate --help**

All should show CLI11-generated help.

---

## Self-Review

1. **Spec coverage:** All spec sections covered — CLI11 (Tasks 4-8), doctest (Tasks 9-10), optional (Task 11), noncopyable (Task 11), in_place (Task 11), shared_ptr/scoped_ptr/scoped_array (Task 12), hash/random/lexical_cast/version.hpp (Task 13), unordered_map/set (Task 14), CMake (Tasks 2-3), CI (Task 16), verification (Task 17). Range header in Task 1, used in Tasks 7, 11.

2. **Placeholder scan:** No TBD/TODO. All steps have concrete code or commands.

3. **Type consistency:** `util::Range<I>` type defined in Task 1 with `begin_`, `end_`, `begin()`, `end()`, `empty()`, `size()`, `front()`, `back()`, `advance_begin(n)`. Used consistently in Tasks 7, 11, 14. `SizeOption` replaced with inline `util::ParseSize` in Tasks 4-8.

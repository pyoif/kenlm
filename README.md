# kenlm-ex

Prebuilt wheels for [KenLM](https://github.com/kpu/kenlm) — C++17, zero Boost, cmake only.

Fork of KenLM by Kenneth Heafield (kenlm at kheafield.com). Packaged with prebuilt wheels for Linux, macOS (universal2), and Windows — no build step required.

## Python

```bash
pip install kenlm-ex
```

```python
import kenlm
model = kenlm.Model('lm/test.arpa')
print(model.score('this is a sentence .', bos=True, eos=True))
```

See [python/example.py](python/example.py) and [python/kenlm.pyx](python/kenlm.pyx) for stateful APIs and full-score iteration.

### Build from source

```bash
MAX_ORDER=6 pip install kenlm-ex --no-binary kenlm-ex
```

Requires cmake ≥ 3.28 and a C++17 compiler.

## C++

### Compile

```bash
mkdir -p build && cd build
cmake -DCOMPILE_TESTS=ON ..
cmake --build . -j4
ctest -j2
```

### Dependencies

| Platform | Packages |
|---|---|
| Ubuntu | `build-essential cmake zlib1g-dev libbz2-dev liblzma-dev` |
| macOS | `brew install cmake eigen` |
| Windows | cmake + MSVC (vcpkg optional) |

Optional compression: zlib (`-DHAVE_ZLIB`), bzip2 (`-DHAVE_BZLIB`), xz (`-DHAVE_XZLIB`).

### Compile options

`KENLM_MAX_ORDER=N` — max n-gram order (default 6). Bakes state into POD struct.

## Tools

| Tool | Purpose |
|---|---|
| `bin/lmplz` | Estimate language model (modified Kneser-Ney) on disk |
| `bin/filter` | Filter ARPA/count file to sentence/phrase vocab |
| `bin/build_binary` | Convert ARPA to mmap-friendly binary |
| `bin/query` | Score sentences from binary model |

### lmplz

```bash
bin/lmplz -o 5 < text > text.arpa
```

### filter

```bash
bin/filter
```

### build_binary

```bash
bin/build_binary trie test.arpa test.klm
```

## Data structures

Two query backends:

| Backend | Speed | Memory | Use |
|---|---|---|---|
| Probing | Fastest | More | Throughput-sensitive |
| Trie | Fast | Least | Memory-constrained |

All probabilities are log base 10. Binary models loaded via mmap.

## Platforms

Tested on x86\_64, ARM (Apple Silicon), PPC64. Runs on Linux, macOS, Windows (MSVC/Cygwin/MinGW).

## API

For decoder integration, use `lm/model.hh` or `lm/virtual_interface.hh`. Runtime tuning in `lm/config.hh`.

## License

LGPL-2.1-or-later. `util/integer_to_string.*` is BSD.

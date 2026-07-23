#ifndef UTIL_STRING_PIECE_HASH_H
#define UTIL_STRING_PIECE_HASH_H

#include <functional>
#include "have.hh"
#include "string_piece.hh"

#ifdef HAVE_ICU
U_NAMESPACE_BEGIN
#endif

inline size_t hash_value(const StringPiece &str) {
  // FNV-1a hash over byte range
  size_t h = 14695981039346656037ULL;
  for (const char *p = str.data(), *end_p = p + str.length(); p != end_p; ++p) {
    h ^= static_cast<size_t>(static_cast<unsigned char>(*p));
    h *= 1099511628211ULL;
  }
  return h;
}

#ifdef HAVE_ICU
U_NAMESPACE_END
#endif

struct StringPieceCompatibleHash {
  size_t operator()(const StringPiece &str) const {
    return hash_value(str);
  }
};

struct StringPieceCompatibleEquals {
  bool operator()(const StringPiece &first, const StringPiece &second) const {
    return first == second;
  }
};

// std::unordered_map std::hash<std::string> works for StringPiece via implicit
// conversion when C++17 transparent hash not available. FindStringPiece
// explictly constructs std::string for lookup compatibility.
template <class T> typename T::const_iterator FindStringPiece(const T &t, const StringPiece &key) {
  return t.find(std::string(key.data(), key.size()));
}

template <class T> typename T::iterator FindStringPiece(T &t, const StringPiece &key) {
  return t.find(std::string(key.data(), key.size()));
}

#endif // UTIL_STRING_PIECE_HASH_H

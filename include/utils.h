#ifndef UTILS_H
#define UTILS_H

#include <deque>
#include <vector>

/**
 * Literal class type that wraps a constant expression string.
 *
 * Uses implicit conversion to allow templates to *seemingly* accept constant
 * strings.
 */
template <size_t N> struct StringLiteral {
  constexpr StringLiteral(const char (&str)[N]) { std::copy_n(str, N, value); }

  char value[N];
};

template <typename... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

// Templates for making a nested pack, which does the transformation
// <T, A, B, C> -> A<B<C<T>>>. This could be useful for defining a pipeline
// of pattern matching for instruction selection where A, B, and C are sorted
// at compile time based on their constexpr cost.

template <class T, template <class> class... Types> struct NestedPackHelper;

template <class T> struct NestedPackHelper<T> {
  typedef T type;
};

template <class T, template <class> class Head, template <class> class... Tail>
struct NestedPackHelper<T, Head, Tail...> {
  typedef Head<typename NestedPackHelper<T, Tail...>::type> type;
};

template <class T, template <class> class... Types>
using NestedPack = typename NestedPackHelper<T, Types...>::type;

template <class T>
using VecDequeVec = NestedPack<T, std::vector, std::deque, std::vector>;

#endif
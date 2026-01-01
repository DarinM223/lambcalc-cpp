#ifndef ARENA_H
#define ARENA_H

#include <cstddef>
#include <cstdint>
#include <new>

namespace lambcalc {
namespace arena {

class Allocator {
  char *begin_;
  char *orig_;
  char *end_;

public:
  Allocator(char *begin, char *end) : begin_(begin), orig_(begin), end_(end) {}
  constexpr Allocator(const Allocator &other) noexcept {
    begin_ = other.begin_;
    orig_ = other.orig_;
    end_ = other.end_;
  }

  template <typename T> T *allocate(std::ptrdiff_t count = 1) {
    std::ptrdiff_t size = sizeof(T);
    std::ptrdiff_t pad =
        -reinterpret_cast<std::uintptr_t>(begin_) & (alignof(T) - 1);
    if (count >= (end_ - begin_ - pad) / size) {
      throw std::bad_alloc();
    }
    void *ptr = begin_ + pad;
    begin_ += pad + count * size;
    return new (ptr) T[count]{};
  }

  void reset() { begin_ = orig_; }
};

template <typename T> class TypedAllocator {
  Allocator &allocator_;

public:
  using value_type = T;
  explicit TypedAllocator(Allocator &allocator) : allocator_(allocator) {}
  template <typename U>
  explicit constexpr TypedAllocator(const TypedAllocator<U> &other) noexcept {
    allocator_ = other.allocator_;
  }
  T *allocate(std::ptrdiff_t n = 1) { return allocator_.allocate<T>(n); }
  void deallocate(T *, std::ptrdiff_t) noexcept {}
  friend bool operator==(const TypedAllocator &a, const TypedAllocator &b) {
    return &a.allocator_ == &b.allocator_;
  }
  friend bool operator!=(const TypedAllocator &a, const TypedAllocator &b) {
    return !(a == b);
  }
};

} // namespace arena
} // namespace lambcalc

#endif
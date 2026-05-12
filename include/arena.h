#ifndef ARENA_H
#define ARENA_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <vector>

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

  template <typename T> T *allocate(ptrdiff_t count = 1) {
    ptrdiff_t size = sizeof(T);
    ptrdiff_t pad = -reinterpret_cast<uintptr_t>(begin_) & (alignof(T) - 1);
    if (count >= (end_ - begin_ - pad) / size) {
      throw std::bad_alloc();
    }
    void *ptr = begin_ + pad;
    begin_ += pad + count * size;
    return new (ptr) T[count]{};
  }

  void reset() { begin_ = orig_; }
};

class LinkedAllocator {
  std::vector<char> buf_;
  Allocator allocator_;
  std::unique_ptr<LinkedAllocator> next_;

public:
  explicit LinkedAllocator(size_t size)
      : buf_(size), allocator_(buf_.data(), buf_.data() + size),
        next_(nullptr) {}

  template <typename T> T *allocate(ptrdiff_t count = 1) {
    try {
      return allocator_.allocate<T>(count);
    } catch (std::bad_alloc const &) {
      if (next_ == nullptr) {
        next_ = std::make_unique<LinkedAllocator>(buf_.capacity());
      }
      return next_->allocate<T>(count);
    }
  }

  void reset() {
    LinkedAllocator *ptr = next_.get();
    while (ptr != nullptr) {
      ptr->allocator_.reset();
      ptr = ptr->next_.get();
    }
  }
};

template <typename T, typename Allocator> class Typed {
  Allocator &allocator_;

public:
  using value_type = T;
  explicit Typed(Allocator &allocator) : allocator_(allocator) {}
  template <typename U>
  explicit constexpr Typed(const Typed<Allocator, U> &other) noexcept
      : allocator_(other.allocator_) {}
  T *allocate(ptrdiff_t n = 1) { return allocator_.template allocate<T>(n); }
  void deallocate(T *, ptrdiff_t) noexcept {}
  friend bool operator==(const Typed &a, const Typed &b) {
    return &a.allocator_ == &b.allocator_;
  }
  friend bool operator!=(const Typed &a, const Typed &b) { return !(a == b); }
};

template <typename T> using TypedAllocator = Typed<T, Allocator>;
template <typename T> using TypedLinkedAllocator = Typed<T, LinkedAllocator>;

} // namespace arena
} // namespace lambcalc

#endif
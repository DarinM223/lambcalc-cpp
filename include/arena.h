#ifndef ARENA_H
#define ARENA_H

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace lambcalc {
namespace arena {

struct allocator {
  char *begin;
  char *end;
};

template <typename T> T *allocate(allocator *alloc, std::ptrdiff_t count = 1) {
  std::ptrdiff_t size = sizeof(T);
  std::ptrdiff_t pad =
      -reinterpret_cast<std::uintptr_t>(alloc->begin) & (alignof(T) - 1);
  assert(count < (alloc->end - alloc->begin - pad) / size);
  void *ptr = alloc->begin + pad;
  alloc->begin += pad + count * size;
  return new (ptr) T[count]{};
}

} // namespace arena
} // namespace lambcalc

#endif
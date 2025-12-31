#include "arena.h"
#include <cstddef>
#include <gtest/gtest.h>
#include <new>
#include <vector>

namespace lambcalc {

using namespace arena;

TEST(Arena, Vector) {
  alignas(alignof(int)) char buf[2000];
  char *ptr = std::launder(buf);
  Allocator allocator(ptr, ptr + sizeof(buf) / sizeof(*buf));
  TypedAllocator<int> typed_allocator(allocator);
  std::vector<int, TypedAllocator<int>> v(typed_allocator);
  std::vector<int> expected;
  for (int i = 0; i <= 100; ++i) {
    v.push_back(i);
    expected.push_back(i);
  }
  for (size_t i = 0; i < v.size(); ++i) {
    EXPECT_EQ(v[i], expected[i]);
  }
}

} // namespace lambcalc
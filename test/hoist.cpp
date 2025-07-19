#include "hoist.h"
#include <gtest/gtest.h>

namespace lambcalc {

using namespace anf;

TEST(Hoist, SimpleJoin) {
  // let join a <x> =
  //   let join b <> = 0
  //   in
  //   let y = x + 1 in
  //   y
  // in
  // let join c <> = 1 in
  // jump a 1
  auto exp = make(JoinExp{
      "a",
      {"x"},
      make(JoinExp{"b",
                   {},
                   make(HaltExp{IntValue{0}}),
                   make(BopExp{"y", ast::Bop::Plus, VarValue{"x"}, IntValue{1},
                               make(HaltExp{VarValue{"y"}})})}),
      make(JoinExp{"c",
                   {},
                   make(HaltExp{IntValue{1}}),
                   make(JumpExp{"a", {IntValue{1}}})})});
  auto collected = anf::hoist(std::move(exp));
  EXPECT_EQ(collected.size(), static_cast<size_t>(1));
}

} // namespace lambcalc
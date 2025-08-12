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
  EXPECT_EQ(collected[0].entryBlock.body->dump(), "JumpExp { a, <1> }");
  auto &blocks = collected[0].blocks;
  EXPECT_EQ(blocks.size(), static_cast<size_t>(3));
  std::pair<std::string, std::string> tests[] = {
      {"b", "HaltExp { 0 }"},
      {"a", "BopExp { y, +, x, 1, HaltExp { y } }"},
      {"c", "HaltExp { 1 }"},
  };
  for (size_t i = 0; i < blocks.size(); ++i) {
    EXPECT_EQ(blocks[i].name, std::get<0>(tests[i]));
    EXPECT_EQ(blocks[i].body->dump(), std::get<1>(tests[i]));
  }
}

TEST(Hoist, FunJoin) {
  // let f1 = fn a =>
  //   let join j1 <> =
  //     let f2 = fn b =>
  //       let join j2 <c> = c in
  //       jump j2 0
  //     in
  //     let y = f2 1 in
  //     y
  //   in
  //   let join j3 <> = jump j1 in
  //   jump j3
  // in
  // let x = f1 0 in
  // x
  auto exp = make(FunExp{
      "f1",
      {"a"},
      make(JoinExp{
          "j1",
          {},
          make(FunExp{
              "f2",
              {"b"},
              make(JoinExp{"j2",
                           {"c"},
                           make(HaltExp{VarValue{"c"}}),
                           make(JumpExp{"j2", {IntValue{0}}})}),
              make(AppExp{
                  "y", "f2", {IntValue{1}}, make(HaltExp{VarValue{"y"}})})}),
          make(JoinExp{
              "j3", {}, make(JumpExp{"j1", {}}), make(JumpExp{"j3", {}})})}),
      make(AppExp{"x", "f1", {IntValue{0}}, make(HaltExp{VarValue{"x"}})})});
  auto collected = anf::hoist(std::move(exp));
  EXPECT_EQ(collected.size(), static_cast<size_t>(3));

  EXPECT_EQ(collected[0].name, "f2");
  EXPECT_EQ(collected[0].entryBlock.body->dump(), "JumpExp { j2, <0> }");
  EXPECT_EQ(collected[0].blocks.size(), static_cast<size_t>(1));
  EXPECT_EQ(collected[0].blocks[0].name, "j2");
  EXPECT_EQ(collected[0].blocks[0].body->dump(), "HaltExp { c }");

  EXPECT_EQ(collected[1].name, "f1");
  EXPECT_EQ(collected[1].entryBlock.body->dump(), "JumpExp { j3, <> }");
  EXPECT_EQ(collected[1].blocks.size(), static_cast<size_t>(2));
  std::pair<std::string, std::string> tests[] = {
      {"j1", "AppExp { y, f2, [1], HaltExp { y } }"},
      {"j3", "JumpExp { j1, <> }"},
  };
  auto &blocks = collected[1].blocks;
  for (size_t i = 0; i < blocks.size(); ++i) {
    EXPECT_EQ(blocks[i].name, std::get<0>(tests[i]));
    EXPECT_EQ(blocks[i].body->dump(), std::get<1>(tests[i]));
  }

  EXPECT_EQ(collected[2].name, "main");
  EXPECT_EQ(collected[2].entryBlock.body->dump(),
            "AppExp { x, f1, [0], HaltExp { x } }");
  EXPECT_EQ(collected[2].blocks.size(), static_cast<size_t>(0));
}

TEST(Hoist, IfElse) {
  auto exp =
      make(IfExp{IntValue{1},
                 make(IfExp{IntValue{0}, make(HaltExp{IntValue{2}}),
                            make(HaltExp{IntValue{4}})}),
                 make(BopExp{"a", ast::Bop::Plus, IntValue{1}, IntValue{2},
                             make(HaltExp{VarValue{"a"}})})});
  auto collected = anf::hoist(std::move(exp));
  EXPECT_EQ(collected.size(), static_cast<size_t>(1));
  EXPECT_EQ(collected[0].blocks.size(), static_cast<size_t>(4));
}

} // namespace lambcalc
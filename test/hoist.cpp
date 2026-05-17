#include "hoist.h"
#include <gtest/gtest.h>

namespace lambcalc {

using namespace anf;

TEST(Hoist, SimpleJoin) {
  SymbolTable table;
  // let join a <x> =
  //   let join b <> = 0
  //   in
  //   let y = x + 1 in
  //   y
  // in
  // let join c <> = 1 in
  // jump a 1
  auto exp = make(JoinExp{
      table.lookup("a"),
      {table.lookup("x")},
      make(JoinExp{table.lookup("b"),
                   {},
                   make(HaltExp{IntValue{0}}),
                   make(BopExp{table.lookup("y"), ast::Bop::Plus,
                               VarValue{table.lookup("x")}, IntValue{1},
                               make(HaltExp{VarValue{table.lookup("y")}})})}),
      make(JoinExp{table.lookup("c"),
                   {},
                   make(HaltExp{IntValue{1}}),
                   make(JumpExp{table.lookup("a"), {IntValue{1}}})})});
  auto collected = anf::hoist(table, std::move(exp));
  EXPECT_EQ(collected.size(), static_cast<size_t>(1));
  EXPECT_EQ(collected[0].entryBlock.body->dump(table), "JumpExp { a, <1> }");
  auto &blocks = collected[0].blocks;
  EXPECT_EQ(blocks.size(), static_cast<size_t>(3));
  std::pair<Symbol, std::string> tests[] = {
      {table.lookup("b"), "HaltExp { 0 }"},
      {table.lookup("a"), "BopExp { y, +, x, 1, HaltExp { y } }"},
      {table.lookup("c"), "HaltExp { 1 }"},
  };
  for (size_t i = 0; i < blocks.size(); ++i) {
    EXPECT_EQ(blocks[i].name, std::get<0>(tests[i]));
    EXPECT_EQ(blocks[i].body->dump(table), std::get<1>(tests[i]));
  }
}

TEST(Hoist, FunJoin) {
  SymbolTable table;
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
      table.lookup("f1"),
      {table.lookup("a")},
      make(JoinExp{
          table.lookup("j1"),
          {},
          make(FunExp{
              table.lookup("f2"),
              {table.lookup("b")},
              make(JoinExp{table.lookup("j2"),
                           {table.lookup("c")},
                           make(HaltExp{VarValue{table.lookup("c")}}),
                           make(JumpExp{table.lookup("j2"), {IntValue{0}}})}),
              make(AppExp{table.lookup("y"),
                          table.lookup("f2"),
                          {IntValue{1}},
                          make(HaltExp{VarValue{table.lookup("y")}})})}),
          make(JoinExp{table.lookup("j3"),
                       {},
                       make(JumpExp{table.lookup("j1"), {}}),
                       make(JumpExp{table.lookup("j3"), {}})})}),
      make(AppExp{table.lookup("x"),
                  table.lookup("f1"),
                  {IntValue{0}},
                  make(HaltExp{VarValue{table.lookup("x")}})})});
  auto collected = anf::hoist(table, std::move(exp));
  EXPECT_EQ(collected.size(), static_cast<size_t>(3));

  EXPECT_EQ(table.lookup(collected[0].name), "f2");
  EXPECT_EQ(collected[0].entryBlock.body->dump(table), "JumpExp { j2, <0> }");
  EXPECT_EQ(collected[0].blocks.size(), static_cast<size_t>(1));
  EXPECT_EQ(table.lookup(collected[0].blocks[0].name), "j2");
  EXPECT_EQ(collected[0].blocks[0].body->dump(table), "HaltExp { c }");

  EXPECT_EQ(table.lookup(collected[1].name), "f1");
  EXPECT_EQ(collected[1].entryBlock.body->dump(table), "JumpExp { j3, <> }");
  EXPECT_EQ(collected[1].blocks.size(), static_cast<size_t>(2));
  std::pair<Symbol, std::string> tests[] = {
      {table.lookup("j1"), "AppExp { y, f2, [1], HaltExp { y } }"},
      {table.lookup("j3"), "JumpExp { j1, <> }"},
  };
  auto &blocks = collected[1].blocks;
  for (size_t i = 0; i < blocks.size(); ++i) {
    EXPECT_EQ(blocks[i].name, std::get<0>(tests[i]));
    EXPECT_EQ(blocks[i].body->dump(table), std::get<1>(tests[i]));
  }

  EXPECT_EQ(table.lookup(collected[2].name), "main");
  EXPECT_EQ(collected[2].entryBlock.body->dump(table),
            "AppExp { x, f1, [0], HaltExp { x } }");
  EXPECT_EQ(collected[2].blocks.size(), static_cast<size_t>(0));
}

TEST(Hoist, IfElse) {
  SymbolTable table;
  auto exp = make(IfExp{
      IntValue{1},
      make(IfExp{IntValue{0}, make(HaltExp{IntValue{2}}),
                 make(HaltExp{IntValue{4}})}),
      make(BopExp{table.lookup("a"), ast::Bop::Plus, IntValue{1}, IntValue{2},
                  make(HaltExp{VarValue{table.lookup("a")}})})});
  auto collected = anf::hoist(table, std::move(exp));
  EXPECT_EQ(collected.size(), static_cast<size_t>(1));
  EXPECT_EQ(collected[0].blocks.size(), static_cast<size_t>(4));
}

} // namespace lambcalc
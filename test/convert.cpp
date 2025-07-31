#include "convert.h"
#include <gtest/gtest.h>
#include <vector>

namespace lambcalc {

using namespace anf;

TEST(FreeVars, Simple) {
  auto exp = make(BopExp{"c", ast::Bop::Plus, VarValue{"a"}, VarValue{"b"},
                         make(HaltExp{VarValue{"c"}})});
  auto set = convert::freeVars(*exp);
  std::vector<Var> vars(set.begin(), set.end());
  std::vector<Var> expected{"a", "b"};
  EXPECT_EQ(vars, expected);
}

TEST(FreeVars, FunJoin) {
  auto exp = make(
      FunExp{"f",
             {"a", "b"},
             make(JoinExp{
                 "j",
                 {"c"},
                 make(TupleExp{
                     "t",
                     {VarValue{"b"}, VarValue{"c"}, VarValue{"d"}},
                     make(ProjExp{"p", "t", 0, make(HaltExp{VarValue{"p"}})})}),
                 make(BopExp{"x", ast::Bop::Plus, VarValue{"a"}, VarValue{"e"},
                             make(JumpExp{"j", {GlobValue{"g"}}})})}),
             make(HaltExp{VarValue{"f"}})});
  auto set = convert::freeVars(*exp);
  std::vector<Var> vars(set.begin(), set.end());
  std::vector<Var> expected{"d", "e", "g"};
  EXPECT_EQ(vars, expected);
}

TEST(ClosureConvert, Simple) {
  // let a = 1 + 2 in
  // let b = 3 * 4 in
  // let f = fn c d =>
  //   let e = a + b in
  //   let g = e + c in
  //   let h = g * d in
  //   h
  // in
  // let i = f 3 a in
  // i
  auto exp = make(BopExp{
      "a", ast::Bop::Plus, IntValue{1}, IntValue{2},
      make(BopExp{
          "b", ast::Bop::Times, IntValue{3}, IntValue{4},
          make(FunExp{
              "f",
              {"c", "d"},
              make(BopExp{
                  "e", ast::Bop::Plus, VarValue{"a"}, VarValue{"b"},
                  make(BopExp{"g", ast::Bop::Plus, VarValue{"e"}, VarValue{"c"},
                              make(BopExp{"h", ast::Bop::Times, VarValue{"g"},
                                          VarValue{"d"},
                                          make(HaltExp{VarValue{"h"}})})})}),
              make(AppExp{"i",
                          "f",
                          {IntValue{3}, VarValue{"a"}},
                          make(HaltExp{VarValue{"i"}})})})})});
  auto convert = convert::closureConvert(std::move(exp));
  // let a = 1 + 2 in
  // let b = 3 * 4 in
  // let f = fn closure0 c d =>
  //   let b = closure0[2] in
  //   let a = closure0[1] in
  //   let e = a + b in
  //   let g = e + c in
  //   let h = g * d in
  //   h
  // in
  // let f = (f, a, b) in
  // let proj1 = f[0] in
  // let i = proj1 f 3 a in
  // i
  EXPECT_EQ(convert->dump(),
            "BopExp { a, +, 1, 2, BopExp { b, *, 3, 4, FunExp { f, [closure0, "
            "c, d], ProjExp { b, closure0, 2, ProjExp { a, closure0, 1, BopExp "
            "{ e, +, a, b, BopExp { g, +, e, c, BopExp { h, *, g, d, HaltExp { "
            "h } } } } } }, TupleExp { f, [f, a, b], ProjExp { proj1, f, 0, "
            "AppExp { i, proj1, [f, 3, a], HaltExp { i } } } } } } }");
}

TEST(ClosureConvert, Nested) {
  // let f1 = fn a =>
  //   let f2 = fn b =>
  //     let r = a + b in
  //     r
  //   in
  //   f2
  // in
  // let t1 = f1 1 in
  // let t2 = t1 2 in
  // t2
  auto exp = make(FunExp{
      "f1",
      {"a"},
      make(FunExp{"f2",
                  {"b"},
                  make(BopExp{"r", ast::Bop::Plus, VarValue{"a"}, VarValue{"b"},
                              make(HaltExp{VarValue{"r"}})}),
                  make(HaltExp{VarValue{"f2"}})}),
      make(AppExp{
          "t1",
          "f1",
          {IntValue{1}},
          make(AppExp{
              "t2", "t1", {IntValue{2}}, make(HaltExp{VarValue{"t2"}})})})});
  auto convert = convert::closureConvert(std::move(exp));
  EXPECT_EQ(convert->dump(),
            "FunExp { f1, [closure0, a], FunExp { f2, [closure3, b], ProjExp { "
            "a, closure3, 1, BopExp { r, +, a, b, HaltExp { r } } }, TupleExp "
            "{ f2, [f2, a], HaltExp { f2 } } }, TupleExp { f1, [f1], ProjExp { "
            "proj1, f1, 0, AppExp { t1, proj1, [f1, 1], ProjExp { proj2, t1, "
            "0, AppExp { t2, proj2, [t1, 2], HaltExp { t2 } } } } } } }");
}

} // namespace lambcalc
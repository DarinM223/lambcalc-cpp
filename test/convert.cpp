#include "convert.h"
#include <gtest/gtest.h>
#include <vector>

namespace lambcalc {

using namespace anf;

TEST(FreeVars, Simple) {
  SymbolTable table;
  auto exp = make(BopExp{
      table.lookup("c"), ast::Bop::Plus, VarValue{table.lookup("a")},
      VarValue{table.lookup("b")}, make(HaltExp{VarValue{table.lookup("c")}})});
  auto set = convert::freeVars(*exp);
  std::vector<Var> vars(set.begin(), set.end());
  std::vector<Var> expected{table.lookup("a"), table.lookup("b")};
  EXPECT_EQ(vars, expected);
}

TEST(FreeVars, FunJoin) {
  SymbolTable table;
  auto exp = make(FunExp{
      table.lookup("f"),
      {table.lookup("a"), table.lookup("b")},
      make(JoinExp{
          table.lookup("j"),
          {table.lookup("c")},
          make(TupleExp{
              table.lookup("t"),
              {VarValue{table.lookup("b")}, VarValue{table.lookup("c")},
               VarValue{table.lookup("d")}},
              make(ProjExp{table.lookup("p"), table.lookup("t"), 0,
                           make(HaltExp{VarValue{table.lookup("p")}})})}),
          make(BopExp{table.lookup("x"), ast::Bop::Plus,
                      VarValue{table.lookup("a")}, VarValue{table.lookup("e")},
                      make(JumpExp{table.lookup("j"),
                                   {GlobValue{table.lookup("g")}}})})}),
      make(HaltExp{VarValue{table.lookup("f")}})});
  auto set = convert::freeVars(*exp);
  std::vector<Var> vars(set.begin(), set.end());
  std::vector<Var> expected{table.lookup("d"), table.lookup("e"),
                            table.lookup("g")};
  EXPECT_EQ(vars, expected);
}

TEST(ClosureConvert, Simple) {
  SymbolTable table;
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
      table.lookup("a"), ast::Bop::Plus, IntValue{1}, IntValue{2},
      make(BopExp{
          table.lookup("b"), ast::Bop::Times, IntValue{3}, IntValue{4},
          make(FunExp{
              table.lookup("f"),
              {table.lookup("c"), table.lookup("d")},
              make(BopExp{
                  table.lookup("e"), ast::Bop::Plus,
                  VarValue{table.lookup("a")}, VarValue{table.lookup("b")},
                  make(BopExp{
                      table.lookup("g"), ast::Bop::Plus,
                      VarValue{table.lookup("e")}, VarValue{table.lookup("c")},
                      make(BopExp{
                          table.lookup("h"), ast::Bop::Times,
                          VarValue{table.lookup("g")},
                          VarValue{table.lookup("d")},
                          make(HaltExp{VarValue{table.lookup("h")}})})})}),
              make(AppExp{table.lookup("i"),
                          table.lookup("f"),
                          {IntValue{3}, VarValue{table.lookup("a")}},
                          make(HaltExp{VarValue{table.lookup("i")}})})})})});
  auto convert = convert::closureConvert(table, std::move(exp));
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
  EXPECT_EQ(convert->dump(table),
            "BopExp { a, +, 1, 2, BopExp { b, *, 3, 4, FunExp { f, [closure0, "
            "c, d], ProjExp { b, closure0, 2, ProjExp { a, closure0, 1, BopExp "
            "{ e, +, a, b, BopExp { g, +, e, c, BopExp { h, *, g, d, HaltExp { "
            "h } } } } } }, TupleExp { f, [f, a, b], ProjExp { proj1, f, 0, "
            "AppExp { i, proj1, [f, 3, a], HaltExp { i } } } } } } }");
}

TEST(ClosureConvert, Nested) {
  SymbolTable table;
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
      table.lookup("f1"),
      {table.lookup("a")},
      make(FunExp{
          table.lookup("f2"),
          {table.lookup("b")},
          make(BopExp{table.lookup("r"), ast::Bop::Plus,
                      VarValue{table.lookup("a")}, VarValue{table.lookup("b")},
                      make(HaltExp{VarValue{table.lookup("r")}})}),
          make(HaltExp{VarValue{table.lookup("f2")}})}),
      make(AppExp{table.lookup("t1"),
                  table.lookup("f1"),
                  {IntValue{1}},
                  make(AppExp{table.lookup("t2"),
                              table.lookup("t1"),
                              {IntValue{2}},
                              make(HaltExp{VarValue{table.lookup("t2")}})})})});
  auto convert = convert::closureConvert(table, std::move(exp));
  EXPECT_EQ(convert->dump(table),
            "FunExp { f1, [closure0, a], FunExp { f2, [closure3, b], ProjExp { "
            "a, closure3, 1, BopExp { r, +, a, b, HaltExp { r } } }, TupleExp "
            "{ f2, [f2, a], HaltExp { f2 } } }, TupleExp { f1, [f1], ProjExp { "
            "proj1, f1, 0, AppExp { t1, proj1, [f1, 1], ProjExp { proj2, t1, "
            "0, AppExp { t2, proj2, [t1, 2], HaltExp { t2 } } } } } } }");
}

} // namespace lambcalc
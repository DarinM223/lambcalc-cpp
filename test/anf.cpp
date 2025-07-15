#include "anf.h"
#include "ast.h"
#include <gtest/gtest.h>

namespace lambcalc {

using namespace ast;

TEST(AnfConversion, BinaryOperators) {
  auto expr = make(BopExp{
      Bop::Plus, make(BopExp{Bop::Times, make(IntExp{2}), make(IntExp{3})}),
      make(IntExp{4})});
  auto anf = anf::convert(*expr);
  EXPECT_EQ(anf->dump(), "BopExp { tmp0, *, 2, 3, BopExp { tmp1, +, tmp0, 4, "
                         "HaltExp { tmp1 } } }");
}

TEST(AnfConversion, LamApp) {
  auto expr =
      make(AppExp{make(LamExp{"x", make(BopExp{Bop::Plus, make(VarExp{"x"}),
                                               make(IntExp{1})})}),
                  make(IntExp{1})});
  auto anf = anf::convert(*expr);
  EXPECT_EQ(anf->dump(),
            "FunExp { tmp2, [x], BopExp { tmp3, +, x, 1, HaltExp { tmp3 } }, "
            "AppExp { tmp4, tmp2, [1], HaltExp { tmp4 } } }");
}

TEST(AnfConversion, IfElse) {
  // (if (if (0 + 1) 0 1) ((fn f => f 1) (fn x => x + 1)) 0)
  auto expr = make(IfExp{
      make(IfExp{make(BopExp{Bop::Plus, make(IntExp{0}), make(IntExp{1})}),
                 make(IntExp{0}), make(IntExp{1})}),
      make(AppExp{
          make(LamExp{"f", make(AppExp{make(VarExp{"f"}), make(IntExp{1})})}),
          make(LamExp{"x", make(BopExp{Bop::Plus, make(VarExp{"x"}),
                                       make(IntExp{1})})})}),
      make(IntExp{0})});
  auto anf = anf::convert(*expr);
  // let tmp5 = 0 + 1 in
  // let join tmp6 <tmp7> =
  //   let join tmp8 <tmp9> = tmp9 in
  //   if tmp7 then
  //     let tmp10 = fn f =>
  //       let tmp11 = f 1 in
  //       tmp11
  //     in
  //     let tmp12 = fn x =>
  //       let tmp13 = x + 1 in
  //       tmp13
  //     in
  //     let tmp14 = tmp10 tmp12 in
  //     jump tmp8 tmp14
  //   else
  //     jump tmp8 0
  // in
  // if tmp5 then jump tmp6 0
  // else jump tmp6 1
  EXPECT_EQ(
      anf->dump(),
      "BopExp { tmp5, +, 0, 1, JoinExp { tmp6, <tmp7>, JoinExp { tmp8, <tmp9>, "
      "HaltExp { tmp9 }, IfExp { tmp7, FunExp { tmp10, [f], AppExp { tmp11, f, "
      "[1], HaltExp { tmp11 } }, FunExp { tmp12, [x], BopExp { tmp13, +, x, 1, "
      "HaltExp { tmp13 } }, AppExp { tmp14, tmp10, [tmp12], JumpExp { tmp8, "
      "<tmp14> } } } }, JumpExp { tmp8, <0> } } }, IfExp { tmp5, JumpExp { "
      "tmp6, <0> }, JumpExp { tmp6, <1> } } } }");
}

} // namespace lambcalc
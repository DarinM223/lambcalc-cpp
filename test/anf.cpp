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
  std::string expected = "BopExp { tmp0, *, 2, 3, BopExp { tmp1, +, tmp0, 4, "
                         "HaltExp { tmp1 } } }";
  EXPECT_EQ(anf->dump(), expected);
}

TEST(AnfConversion, LamApp) {
  auto expr =
      make(AppExp{make(LamExp{"x", make(BopExp{Bop::Plus, make(VarExp{"x"}),
                                               make(IntExp{1})})}),
                  make(IntExp{1})});
  anf::resetCounter();
  auto anf = anf::convert(*expr);
  std::string expected =
      "FunExp { tmp1, [x], BopExp { tmp0, +, x, 1, HaltExp { tmp0 } }, AppExp "
      "{ tmp2, tmp1, [1], HaltExp { tmp2 } } }";
  EXPECT_EQ(anf->dump(), expected);
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
  anf::resetCounter();
  auto anf = anf::convert(*expr);
  // let tmp0 = 0 + 1 in
  // let join tmp1 <tmp2> =
  //   let join tmp3 <tmp4> = tmp4 in
  //   if tmp2 then
  //     let tmp6 = fn f =>
  //       let tmp5 = f 1 in
  //       tmp5
  //     in
  //     let tmp8 = fn x =>
  //       let tmp7 = x + 1 in
  //       tmp7
  //     in
  //     let tmp9 = tmp6 tmp8 in
  //     jump tmp3 tmp9
  //   else
  //     jump tmp3 0
  // in
  // if tmp0 then jump tmp1 0
  // else jump tmp1 1
  std::string expected =
      "BopExp { tmp0, +, 0, 1, JoinExp { tmp1, <tmp2>, JoinExp { tmp3, <tmp4>, "
      "HaltExp { tmp4 }, IfExp { tmp2, FunExp { tmp6, [f], AppExp { tmp5, f, "
      "[1], HaltExp { tmp5 } }, FunExp { tmp8, [x], BopExp { tmp7, +, x, 1, "
      "HaltExp { tmp7 } }, AppExp { tmp9, tmp6, [tmp8], JumpExp { tmp3, <tmp9> "
      "} } } }, JumpExp { tmp3, <0> } } }, IfExp { tmp0, JumpExp { tmp1, <0> "
      "}, JumpExp { tmp1, <1> } } } }";
  EXPECT_EQ(anf->dump(), expected);

  anf::resetCounter();
  auto anfDefunc = anf::convertDefunc(*expr);
  EXPECT_EQ(anf->dump(), anfDefunc->dump());
}

TEST(AnfConversion, NoStackOverflow) {
  auto exp = make(IntExp{1});
  for (size_t i = 0; i < 2000; ++i) {
    exp = make(BopExp{Bop::Plus, make(IntExp{1}), std::move(exp)});
  }

  // Modifying this to use anf::convert should lead to stack overflow, otherwise
  // increase the size of exp.
  anf::resetCounter();
  auto anf = anf::convertDefunc(*exp);
  EXPECT_EQ(anf->dump().size(), 67793);
}

} // namespace lambcalc
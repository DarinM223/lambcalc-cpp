#include "rename.h"
#include <gtest/gtest.h>

namespace lambcalc {

using namespace ast;

TEST(AlphaRename, Simple) {
  // (fn a => (fn b => b + 1) 1 + (fn b => b + 1) 2) 3
  auto exp = make(AppExp{
      make(LamExp{
          "a", make(BopExp{
                   Bop::Plus,
                   make(AppExp{make(LamExp{"b", make(BopExp{Bop::Plus,
                                                            make(VarExp{"b"}),
                                                            make(IntExp{1})})}),
                               make(IntExp{1})}),
                   make(AppExp{make(LamExp{"b", make(BopExp{Bop::Plus,
                                                            make(VarExp{"b"}),
                                                            make(IntExp{1})})}),
                               make(IntExp{2})})})}),
      make(IntExp{3})});
  rename(*exp);
  std::string expected =
      "((fn a0 => (((fn b2 => (b2 + 1)) 1) + ((fn b1 => (b1 + 1)) 2))) 3)";
  EXPECT_EQ(exp->dump(), expected);
}

TEST(AlphaRename, RestoresBindingOnLamExit) {
  // (fn a => (a + (fn a => a + 1) 1) + a) 2
  auto exp = make(AppExp{
      make(LamExp{
          "a",
          make(BopExp{
              Bop::Plus,
              make(BopExp{
                  Bop::Plus, make(VarExp{"a"}),
                  make(AppExp{
                      make(LamExp{"a", make(BopExp{Bop::Plus, make(VarExp{"a"}),
                                                   make(IntExp{1})})}),
                      make(IntExp{1})})}),
              make(VarExp{"a"})})}),
      make(IntExp{2})});
  rename(*exp);
  std::string expected = "((fn a0 => ((a0 + ((fn a1 => (a1 + 1)) 1)) + a0)) 2)";
  EXPECT_EQ(exp->dump(), expected);
}

} // namespace lambcalc
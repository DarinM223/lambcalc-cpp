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

} // namespace lambcalc
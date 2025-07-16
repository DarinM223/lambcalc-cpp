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

} // namespace lambcalc
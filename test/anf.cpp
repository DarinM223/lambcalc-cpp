#include "anf.h"
#include "ast.h"
#include <gtest/gtest.h>

namespace lambcalc {

TEST(Hello, Hello) {
  using namespace ast;
  auto expr = std::make_unique<Exp>(BopExp{
      Bop::Plus,
      std::make_unique<Exp>(BopExp{Bop::Times, std::make_unique<Exp>(IntExp{2}),
                                   std::make_unique<Exp>(IntExp{3})}),
      std::make_unique<Exp>(IntExp{4})});

  auto anf = anf::convert(*expr);

  EXPECT_EQ(anf->dump(), "BopExp { tmp0, 2, 2, 3, BopExp { tmp1, 0, tmp0, 4, "
                         "HaltExp { tmp1 } } }");
}

} // namespace lambcalc
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

  EXPECT_EQ(anf, nullptr);
}

} // namespace lambcalc
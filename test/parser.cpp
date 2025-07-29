#include "parser.h"
#include <gtest/gtest.h>
#include <sstream>

namespace lambcalc {

const std::unordered_map<ast::Bop, std::optional<std::pair<int, int>>>
    defaultInfixBp{{ast::Bop::Plus, {{1, 2}}},
                   {ast::Bop::Minus, {{1, 2}}},
                   {ast::Bop::Times, {{3, 4}}}};

TEST(Parser, ParsesOperators) {
  std::istringstream is(" (fn a => a + 1 + 2 * 3 * 4 + 5 ) ");
  Lexer lexer(is);
  Parser parser(lexer, defaultInfixBp);
  auto exp = parser.parseExpression();
  std::string expected = "(fn a => (((a + 1) + ((2 * 3) * 4)) + 5))";
  EXPECT_EQ(exp->dump(), expected);
}

TEST(Parser, ParsesApp) {
  std::istringstream is("a b c + d e f");
  Lexer lexer(is);
  Parser parser(lexer, defaultInfixBp);
  auto exp = parser.parseExpression();
  std::string expected = "(((a b) c) + ((d e) f))";
  EXPECT_EQ(exp->dump(), expected);
}

TEST(Parser, ParseIfWithAppParens) {
  std::istringstream is("if x then x * f (x - 1) else 1");
  Lexer lexer(is);
  Parser parser(lexer, defaultInfixBp);
  auto exp = parser.parseExpression();
  std::string expected = "(if x then (x * (f (x - 1))) else 1)";
  EXPECT_EQ(exp->dump(), expected);
}

TEST(Parser, ZCombinator) {
  std::istringstream is(
      "(fn g => (fn x => g (fn v => x x v)) (fn x => g (fn v => x x v))) (fn f "
      "=> fn x => if x then (if x - 1 then x * f (x - 1) else 1) else 1) 5");
  Lexer lexer(is);
  Parser parser(lexer, defaultInfixBp);
  auto exp = parser.parseExpression();
  std::string expected =
      "(((fn g => ((fn x => (g (fn v => ((x x) v)))) (fn x => (g (fn v => ((x "
      "x) v)))))) (fn f => (fn x => (if x then (if (x - 1) then (x * (f (x - "
      "1))) else 1) else 1)))) 5)";
  EXPECT_EQ(exp->dump(), expected);
}

} // namespace lambcalc
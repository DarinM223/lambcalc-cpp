#include "lexer.h"
#include <gtest/gtest.h>
#include <sstream>

namespace lambcalc {

TEST(Lexer, Tokens) {
  std::istringstream is(" (fn a => a + 1 ) ( if 1  then  2 else  3)  ");
  Lexer lexer(is);
  std::vector<std::string> tokens;
  Token token;
  while ((token = lexer.getToken()) != Token::Eof) {
    switch (token) {
    case Token::Number:
      tokens.push_back(std::to_string(lexer.getNumber()));
      break;
    case Token::Identifier:
      tokens.emplace_back(lexer.getIdentifier());
      break;
    case Token::Fn:
      tokens.emplace_back("fn");
      break;
    case Token::Arrow:
      tokens.emplace_back("=>");
      break;
    case Token::If:
      tokens.emplace_back("if");
      break;
    case Token::Then:
      tokens.emplace_back("then");
      break;
    case Token::Else:
      tokens.emplace_back("else");
      break;
    default:
      tokens.emplace_back(1, static_cast<char>(token));
      break;
    }
  }
  std::vector<std::string> expected{"(", "fn",   "a", "=>", "a", "+",
                                    "1", ")",    "(", "if", "1", "then",
                                    "2", "else", "3", ")"};
  EXPECT_EQ(tokens, expected);
}

} // namespace lambcalc
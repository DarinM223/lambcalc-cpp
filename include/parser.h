#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include <optional>

namespace lambcalc {

class Parser {
  Lexer &lexer_;
  Token currentToken_;
  std::optional<Token> peekToken_;
  std::unordered_map<ast::Bop, std::optional<std::pair<int, int>>> infixBp_;

  std::unique_ptr<ast::Exp> parseFn();
  std::unique_ptr<ast::Exp> parseIf();
  std::unique_ptr<ast::Exp> parseParens();
  std::unique_ptr<ast::Exp> parsePrimary();
  std::unique_ptr<ast::Exp> parseBinOp(int minBP);

public:
  Parser(
      Lexer &lexer,
      std::unordered_map<ast::Bop, std::optional<std::pair<int, int>>> infixBp)
      : lexer_(lexer), currentToken_(Token::Eof), infixBp_(std::move(infixBp)) {
  }
  Token getCurrentToken() { return currentToken_; }
  void nextToken() {
    if (peekToken_) {
      currentToken_ = *peekToken_;
      peekToken_ = std::nullopt;
    } else {
      currentToken_ = lexer_.getToken();
    }
  }
  Token peekToken() {
    if (peekToken_) {
      return *peekToken_;
    }
    return *(peekToken_ = lexer_.getToken());
  }
  std::unique_ptr<ast::Exp> parseExpression();
};

} // namespace lambcalc

#endif
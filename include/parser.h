#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include <optional>

namespace lambcalc {

class ParserException : public std::exception {
  std::string reason_;
  bool abort_;

public:
  explicit ParserException(const std::string &reason, bool abort = false)
      : reason_(reason), abort_(abort) {}
  bool abort() noexcept { return abort_; }
  virtual const char *what() const throw() { return reason_.c_str(); }
};

class Parser {
  Lexer &lexer_;
  Token currentToken_;
  std::optional<Token> peekToken_;
  std::unordered_map<ast::Bop, std::optional<std::pair<int, int>>> infixBp_;

  std::unique_ptr<ast::Exp<>> parseFn();
  std::unique_ptr<ast::Exp<>> parseIf();
  std::unique_ptr<ast::Exp<>> parseParens();
  std::unique_ptr<ast::Exp<>> parsePrimary();
  std::unique_ptr<ast::Exp<>> parseBinOp(int minBP);

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
  std::optional<Token> getPeekToken() { return peekToken_; }
  Token peekToken() {
    return peekToken_ ? *peekToken_ : *(peekToken_ = lexer_.getToken());
  }
  std::unique_ptr<ast::Exp<>> parseExpression();
};

} // namespace lambcalc

#endif
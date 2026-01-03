#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include <memory>
#include <optional>
#include <utility>

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

template <template <class> class Ptr = std::unique_ptr,
          typename Allocator = std::allocator<ast::Exp<Ptr>>>
class Parser {
  Allocator &allocator_;
  Lexer &lexer_;
  Token currentToken_;
  std::optional<Token> peekToken_;
  std::unordered_map<ast::Bop, std::optional<std::pair<int, int>>> infixBp_;

  Ptr<ast::Exp<Ptr>> parseFn();
  Ptr<ast::Exp<Ptr>> parseIf();
  Ptr<ast::Exp<Ptr>> parseParens();
  Ptr<ast::Exp<Ptr>> parsePrimary();
  Ptr<ast::Exp<Ptr>> parseBinOp(int minBP);

  Ptr<ast::Exp<Ptr>> make(ast::Exp<Ptr> &&exp) {
    ast::Exp<Ptr> *ptr = allocator_.allocate(1);
    std::allocator_traits<Allocator>::construct(allocator_, ptr,
                                                std::move(exp));
    Ptr<ast::Exp<Ptr>> ptr2(ptr);
    return ptr2;
  }

public:
  Parser(
      Allocator &allocator, Lexer &lexer,
      std::unordered_map<ast::Bop, std::optional<std::pair<int, int>>> infixBp)
      : allocator_(allocator), lexer_(lexer), currentToken_(Token::Eof),
        infixBp_(std::move(infixBp)) {}
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
  Ptr<ast::Exp<Ptr>> parseExpression();
};

} // namespace lambcalc

#endif
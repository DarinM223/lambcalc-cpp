#include "parser.h"
#include "arena.h"
#include "utils.h"
#include <cassert>
#include <memory>

namespace lambcalc {

static std::optional<ast::Bop> parseOp(const Token &token) {
  switch (token) {
  case Token::Plus:
    return ast::Bop::Plus;
  case Token::Minus:
    return ast::Bop::Minus;
  case Token::Times:
    return ast::Bop::Times;
  default:
    return std::nullopt;
  }
}

template <template <class> class Ptr, typename Allocator>
Ptr<ast::Exp<Ptr>> Parser<Ptr, Allocator>::parseFn() {
  nextToken();
  assert(getCurrentToken() == Token::Identifier &&
         "Expected variable parameter");
  auto param = lexer_.getIdentifier();
  nextToken();
  assert(getCurrentToken() == Token::Arrow &&
         "Expected arrow after function parameter");
  auto body = parseExpression();
  return make(ast::LamExp<Ptr>{std::move(param), std::move(body)});
}

template <template <class> class Ptr, typename Allocator>
Ptr<ast::Exp<Ptr>> Parser<Ptr, Allocator>::parseIf() {
  auto cond = parseExpression();
  nextToken();
  assert(getCurrentToken() == Token::Then &&
         "Expected then after if condition");
  auto then = parseExpression();
  nextToken();
  assert(getCurrentToken() == Token::Else &&
         "Expected else after then expression");
  auto els = parseExpression();
  return make(
      ast::IfExp<Ptr>{std::move(cond), std::move(then), std::move(els)});
}

template <template <class> class Ptr, typename Allocator>
Ptr<ast::Exp<Ptr>> Parser<Ptr, Allocator>::parseParens() {
  auto exp = parseExpression();
  nextToken();
  assert(getCurrentToken() == Token::RParen && "Expected right parenthesis");
  return exp;
}

template <template <class> class Ptr, typename Allocator>
Ptr<ast::Exp<Ptr>> Parser<Ptr, Allocator>::parsePrimary() {
  switch (getCurrentToken()) {
  case Token::LParen:
    return parseParens();
  case Token::Number:
    return make(ast::IntExp{lexer_.getNumber()});
  case Token::Fn:
    return parseFn();
  case Token::If:
    return parseIf();
  case Token::Identifier:
    return make(ast::VarExp{lexer_.getIdentifier()});
  default:
    throw ParserException(
        "Invalid token: " + std::to_string(static_cast<int>(getCurrentToken())),
        getCurrentToken() == Token::Eof);
  }
}

constexpr int baseBP = 0;

template <template <class> class Ptr, typename Allocator>
Ptr<ast::Exp<Ptr>> Parser<Ptr, Allocator>::parseBinOp(int minBP) {
  nextToken();
  Ptr<ast::Exp<Ptr>> lhs = parsePrimary();

  int appLbp = 100, appRbp = 101;
  while (true) {
    Token token = peekToken();
    std::optional<ast::Bop> bop;
    if ((bop = parseOp(token))) {
      std::optional<std::pair<int, int>> bp;
      if ((bp = infixBp_[*bop])) {
        auto [lbp, rbp] = *bp;
        if (lbp < minBP) {
          return lhs;
        }

        nextToken();
        auto rhs = parseBinOp(rbp);
        lhs = make(ast::BopExp<Ptr>{*bop, std::move(lhs), std::move(rhs)});
      } else {
        return lhs;
      }
    } else if (token == Token::LParen || token == Token::Number ||
               token == Token::Identifier) {
      // If lookahead is '(', variable, or number, apply function application.
      if (appLbp < minBP) {
        return lhs;
      }

      auto rhs = parseBinOp(appRbp);
      lhs = make(ast::AppExp<Ptr>{std::move(lhs), std::move(rhs)});
    } else {
      return lhs;
    }
  }
}

template <template <class> class Ptr, typename Allocator>
Ptr<ast::Exp<Ptr>> Parser<Ptr, Allocator>::parseExpression() {
  return parseBinOp(baseBP);
}

template class Parser<std::unique_ptr, std::allocator<ast::Exp<>>>;
template class Parser<raw_ptr, arena::TypedAllocator<ast::Exp<raw_ptr>>>;

} // namespace lambcalc
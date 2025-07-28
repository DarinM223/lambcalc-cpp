#include "parser.h"
#include <cassert>

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

std::unique_ptr<ast::Exp> Parser::parseFn() {
  nextToken();
  assert(getCurrentToken() == Token::Identifier &&
         "Expected variable parameter");
  auto param = lexer_.getIdentifier();
  nextToken();
  assert(getCurrentToken() == Token::Arrow &&
         "Expected arrow after function parameter");
  auto body = parseExpression();
  return ast::make(ast::LamExp{std::move(param), std::move(body)});
}

std::unique_ptr<ast::Exp> Parser::parseIf() {
  auto cond = parseExpression();
  assert(getCurrentToken() == Token::Then &&
         "Expected then after if condition");
  nextToken();
  auto then = parseExpression();
  assert(getCurrentToken() == Token::Else &&
         "Expected else after then expression");
  nextToken();
  auto els = parseExpression();
  return ast::make(
      ast::IfExp{std::move(cond), std::move(then), std::move(els)});
}

std::unique_ptr<ast::Exp> Parser::parseParens() {
  auto exp = parseExpression();
  nextToken();
  assert(getCurrentToken() == Token::RParen && "Expected right parenthesis");
  return exp;
}

std::unique_ptr<ast::Exp> Parser::parsePrimary() {
  switch (getCurrentToken()) {
  case Token::LParen:
    return parseParens();
  case Token::Number:
    return ast::make(ast::IntExp{lexer_.getNumber()});
  case Token::Fn:
    return parseFn();
  case Token::If:
    return parseIf();
  case Token::Identifier:
    return ast::make(ast::VarExp{lexer_.getIdentifier()});
  default:
    throw std::runtime_error(
        "Invalid token: " +
        std::to_string(static_cast<int>(getCurrentToken())));
  }
}

std::unique_ptr<ast::Exp> Parser::parseBinOp(int minBP) {
  nextToken();
  std::unique_ptr<ast::Exp> lhs = parsePrimary();

  while (true) {
    Token token = getCurrentToken();
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
        lhs = ast::make(ast::BopExp{*bop, std::move(lhs), std::move(rhs)});
      } else {
        return lhs;
      }
    } else if (token == Token::Eof) {
      return lhs;
    } else {
      throw std::runtime_error("Invalid token: " +
                               std::to_string(static_cast<int>(token)));
    }
  }
}

std::unique_ptr<ast::Exp> Parser::parseExpression() { return parseBinOp(0); }

} // namespace lambcalc
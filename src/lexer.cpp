#include "lexer.h"
#include <iostream>

namespace lambcalc {

Token Lexer::getToken() {
  while (isspace(lastChar_)) {
    lastChar_ = in_.get();
  }
  if (lastChar_ == '=' && in_.peek() == '>') {
    in_.get();
    lastChar_ = in_.get();
    return Token::Arrow;
  }
  if (isalpha(lastChar_)) {
    std::string identifier(1, lastChar_);
    while (isalnum((lastChar_ = in_.get()))) {
      identifier += lastChar_;
    }
    identifier_ = table_.lookup(identifier);
    if (identifier == "fn") {
      return Token::Fn;
    }
    if (identifier == "=>") {
      return Token::Arrow;
    }
    if (identifier == "if") {
      return Token::If;
    }
    if (identifier == "then") {
      return Token::Then;
    }
    if (identifier == "else") {
      return Token::Else;
    }
    return Token::Identifier;
  }
  if (isdigit(lastChar_)) {
    std::string numStr;
    do {
      numStr += lastChar_;
      lastChar_ = in_.get();
    } while (isdigit(lastChar_));
    numberValue_ = strtod(numStr.c_str(), 0);
    return Token::Number;
  }
  if (lastChar_ == EOF) {
    return Token::Eof;
  }
  Token token = static_cast<Token>(lastChar_);
  lastChar_ = in_.get();
  return token;
}

} // namespace lambcalc
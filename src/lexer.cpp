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
    std::string identifier;
    do {
      identifier_ += lastChar_;
    } while (isalnum((lastChar_ = in_.get())));
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
    identifier_ = table_.lookup(identifier);
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
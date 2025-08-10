#ifndef LEXER_H
#define LEXER_H

#include <string>

namespace lambcalc {

enum class Token {
  Eof = -1,
  Number = -2,
  Identifier = -3,
  Fn = -4,
  Arrow = -5,
  If = -6,
  Then = -7,
  Else = -8,
  LParen = '(',
  RParen = ')',
  Plus = '+',
  Minus = '-',
  Times = '*',
  Semicolon = ';',
};

class Lexer {
  std::istream &in_;
  int lastChar_;
  std::string identifier_;
  int numberValue_;

public:
  explicit Lexer(std::istream &in) : in_(in), lastChar_(' '), numberValue_(0) {}
  Token getToken();
  const std::string &getIdentifier() const { return identifier_; }
  int getNumber() const { return numberValue_; }
};

} // namespace lambcalc

#endif
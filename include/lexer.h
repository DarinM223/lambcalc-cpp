#ifndef LEXER_H
#define LEXER_H

#include "symbol.h"

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
  SymbolTable &table_;
  std::istream &in_;
  int lastChar_;
  Symbol identifier_;
  int numberValue_;

public:
  explicit Lexer(SymbolTable &table, std::istream &in)
      : table_(table), in_(in), lastChar_(' '), numberValue_(0) {}
  Token getToken();
  const Symbol &getIdentifier() const { return identifier_; }
  int getNumber() const { return numberValue_; }
};

} // namespace lambcalc

#endif
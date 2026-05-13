#ifndef SYMBOL_H
#define SYMBOL_H

#include <string>
#include <unordered_map>

namespace lambcalc {

using Symbol = size_t;

class SymbolTable {
  size_t counter_;
  std::unordered_map<Symbol, std::string> symToStr_;
  std::unordered_map<std::string, Symbol> strToSym_;

public:
  SymbolTable() : counter_(0) {};
  Symbol lookup(const std::string &s) {
    if (strToSym_.contains(s)) {
      return strToSym_[s];
    }
    Symbol sym = counter_++;
    strToSym_[s] = sym;
    symToStr_[sym] = s;
    return sym;
  }
  const std::string &lookup(Symbol sym) const { return symToStr_.at(sym); };
};

} // namespace lambcalc

#endif // SYMBOL_H
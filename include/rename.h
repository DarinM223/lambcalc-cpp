#ifndef RENAME_H
#define RENAME_H

#include "ast.h"

namespace lambcalc {
namespace ast {

class NotInScopeException : public std::exception {
  std::string var_;
  std::string reason_;

public:
  explicit NotInScopeException(const std::string &var)
      : var_(var), reason_(var + " is not in scope") {}
  const std::string &var() { return var_; }
  virtual const char *what() const throw() { return reason_.c_str(); }
};

void rename(ast::Exp<> &exp);

} // namespace ast
} // namespace lambcalc

#endif
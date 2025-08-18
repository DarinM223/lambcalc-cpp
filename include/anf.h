#ifndef ANF_H
#define ANF_H

#include "ast.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace lambcalc {
namespace anf {

struct Value;
struct Exp;

using Cont = std::function<std::unique_ptr<Exp>(Value)>;
using Var = std::string;

struct IntValue {
  int value;
};

struct VarValue {
  Var var;
};

struct GlobValue {
  Var glob;
};

struct Value : public std::variant<IntValue, VarValue, GlobValue> {
  using variant::variant;
  friend std::ostream &operator<<(std::ostream &os, const Value &value);
};

struct HaltExp {
  Value value;
};

struct FunExp {
  Var name;
  std::vector<Var> params;
  std::unique_ptr<Exp> body;
  std::unique_ptr<Exp> rest;
};

struct JoinExp {
  Var name;
  std::optional<Var> slot;
  std::unique_ptr<Exp> body;
  std::unique_ptr<Exp> rest;
};

struct JumpExp {
  Var joinName;
  std::optional<Value> slotValue;
};

struct AppExp {
  Var name;
  Var funName;
  std::vector<Value> paramValues;
  std::unique_ptr<Exp> rest;
};

struct BopExp {
  Var name;
  ast::Bop bop;
  Value param1;
  Value param2;
  std::unique_ptr<Exp> rest;
};

struct IfExp {
  Value cond;
  std::unique_ptr<Exp> thenBranch;
  std::unique_ptr<Exp> elseBranch;
};

struct TupleExp {
  Var name;
  std::vector<Value> values;
  std::unique_ptr<Exp> rest;
};

struct ProjExp {
  Var name;
  Var tuple;
  int index;
  std::unique_ptr<Exp> rest;
};

struct Exp : public std::variant<HaltExp, FunExp, JoinExp, JumpExp, AppExp,
                                 BopExp, IfExp, TupleExp, ProjExp> {
  using variant::variant;
  friend std::ostream &operator<<(std::ostream &os, const Exp &exp);
  std::string dump();
  // Need to redefine the move constructor after defining the destructor :(
  Exp(Exp &&) = default;
  ~Exp();
};

void resetCounter();
std::unique_ptr<Exp> make(Exp &&exp);
std::unique_ptr<Exp> convert(ast::Exp &exp);
std::unique_ptr<Exp> convertDefunc(ast::Exp &root);

} // namespace anf
} // namespace lambcalc

#endif
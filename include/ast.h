#ifndef AST_H
#define AST_H

#include <functional>
#include <memory>
#include <string>
#include <variant>

namespace lambcalc {
namespace anf {
struct Value;
struct Exp;
} // namespace anf
namespace ast {

struct Exp;

struct IntExp {
  int value;
};

struct VarExp {
  std::string name;
};

struct LamExp {
  std::string param;
  std::unique_ptr<Exp> body;
};

struct AppExp {
  std::unique_ptr<Exp> fn;
  std::unique_ptr<Exp> arg;
};

enum class Bop {
  Plus,
  Minus,
  Times,
};

struct BopExp {
  Bop bop;
  std::unique_ptr<Exp> arg1;
  std::unique_ptr<Exp> arg2;
};

struct IfExp {
  std::unique_ptr<Exp> cond;
  std::unique_ptr<Exp> then;
  std::unique_ptr<Exp> els;
};

struct Exp
    : public std::variant<IntExp, VarExp, LamExp, AppExp, BopExp, IfExp> {
  using variant::variant;
  friend std::ostream &operator<<(std::ostream &os, const Exp &exp);
  std::string dump();

  std::unique_ptr<anf::Exp>
  convert(std::function<std::unique_ptr<anf::Exp>(anf::Value)> k);
};

std::unique_ptr<Exp> make(Exp &&exp);

} // namespace ast
} // namespace lambcalc

#endif
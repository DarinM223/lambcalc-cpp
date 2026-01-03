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

template <template <class> class Ptr = std::unique_ptr> struct Exp;

struct IntExp {
  int value;
};

struct VarExp {
  std::string name;
};

template <template <class> class Ptr> struct LamExp {
  std::string param;
  Ptr<Exp<Ptr>> body;
};

template <template <class> class Ptr> struct AppExp {
  Ptr<Exp<Ptr>> fn;
  Ptr<Exp<Ptr>> arg;
};

enum class Bop {
  Plus,
  Minus,
  Times,
};

template <template <class> class Ptr> struct BopExp {
  Bop bop;
  Ptr<Exp<Ptr>> arg1;
  Ptr<Exp<Ptr>> arg2;
};

template <template <class> class Ptr> struct IfExp {
  Ptr<Exp<Ptr>> cond;
  Ptr<Exp<Ptr>> then;
  Ptr<Exp<Ptr>> els;
};

template <template <class> class Ptr>
struct Exp : public std::variant<IntExp, VarExp, LamExp<Ptr>, AppExp<Ptr>,
                                 BopExp<Ptr>, IfExp<Ptr>> {
  using std::variant<IntExp, VarExp, LamExp<Ptr>, AppExp<Ptr>, BopExp<Ptr>,
                     IfExp<Ptr>>::variant;

  template <template <class> class P>
  friend std::ostream &operator<<(std::ostream &os, const Exp<P> &exp);
  std::string dump();

  // Need to redefine the move constructor after defining the destructor :(
  Exp(Exp<Ptr> &&) = default;
  ~Exp();
};

std::unique_ptr<Exp<>> make(Exp<> &&exp);
std::unique_ptr<anf::Exp>
convert(Exp<> &exp, std::function<std::unique_ptr<anf::Exp>(anf::Value)> k);

} // namespace ast
} // namespace lambcalc

#endif
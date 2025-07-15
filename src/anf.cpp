#include "anf.h"
#include <exception>
#include <sstream>

namespace lambcalc {
namespace anf {

static int counter = 0;
std::string fresh() { return std::string("tmp") + std::to_string(counter++); }

/**
 * Literal class type that wraps a constant expression string.
 *
 * Uses implicit conversion to allow templates to *seemingly* accept constant
 * strings.
 */
template <size_t N> struct StringLiteral {
  constexpr StringLiteral(const char (&str)[N]) { std::copy_n(str, N, value); }

  char value[N];
};

template <StringLiteral lit> struct StringValueVisitor {
  std::string operator()(VarValue v) { return v.var; }
  std::string operator()(GlobValue v) { return v.glob; }
  std::string operator()(auto) {
    throw std::runtime_error(std::string(lit.value) +
                             " expected to return var or glob");
  }
};

struct AnfConvertVisitor {
  Cont k;
  std::unique_ptr<Exp> operator()(ast::IntExp &exp) {
    return k(IntValue{exp.value});
  }
  std::unique_ptr<Exp> operator()(ast::VarExp &exp) {
    return k(VarValue{exp.name});
  }
  std::unique_ptr<Exp> operator()(ast::LamExp &exp) {
    auto name = fresh();
    return make(FunExp{
        .name = name,
        .params = {exp.param},
        .body = convert(*exp.body),
        .rest = k(VarValue{name}),
    });
  }
  std::unique_ptr<Exp> operator()(ast::AppExp &exp) {
    return exp.fn->convert([&arg = *exp.arg, &k = k](Value fnValue) {
      std::string fnName =
          std::visit(StringValueVisitor<"function">{}, std::move(fnValue));
      return arg.convert([fnName = std::move(fnName), &k = k](Value argValue) {
        auto name = fresh();
        return make(AppExp{
            .name = name,
            .funName = fnName,
            .paramValues = {argValue},
            .rest = k(VarValue{name}),
        });
      });
    });
  }
  std::unique_ptr<Exp> operator()(ast::BopExp &exp) {
    return exp.arg1->convert(
        [bop = exp.bop, &arg2 = *exp.arg2, &k = k](Value arg1Value) {
          return arg2.convert(
              [bop, arg1Value = std::move(arg1Value), &k = k](Value arg2Value) {
                auto name = fresh();
                return make(BopExp{
                    .name = name,
                    .bop = bop,
                    .param1 = arg1Value,
                    .param2 = arg2Value,
                    .rest = k(VarValue{name}),
                });
              });
        });
  }
  std::unique_ptr<Exp> operator()(ast::IfExp &exp) {
    return exp.cond->convert([&thenBranch = *exp.then, &elseBranch = *exp.els,
                              &k = k](Value condValue) {
      auto joinName = fresh();
      auto slot = fresh();
      return make(JoinExp{
          .name = joinName,
          .slot = std::optional{slot},
          .body = k(VarValue{slot}),
          .rest = make(IfExp{
              .cond = condValue,
              .thenBranch = thenBranch.convert([&joinName =
                                                    joinName](Value value) {
                return make(JumpExp{joinName, std::optional{std::move(value)}});
              }),
              .elseBranch = elseBranch.convert([&joinName =
                                                    joinName](Value value) {
                return make(JumpExp{joinName, std::optional{std::move(value)}});
              })})});
    });
  }
};

std::unique_ptr<Exp> make(Exp &&exp) {
  return std::make_unique<Exp>(std::move(exp));
}

std::unique_ptr<Exp> convert(ast::Exp &exp) {
  return exp.convert([](Value value) { return make(HaltExp{value}); });
}

std::string Exp::dump() {
  std::ostringstream out;
  out << *this;
  return out.str();
}

} // namespace anf

namespace ast {
std::unique_ptr<Exp> make(Exp &&exp) {
  return std::make_unique<Exp>(std::move(exp));
}
std::unique_ptr<anf::Exp> Exp::convert(anf::Cont k) {
  return std::visit(anf::AnfConvertVisitor{.k = std::move(k)}, *this);
}
} // namespace ast
} // namespace lambcalc
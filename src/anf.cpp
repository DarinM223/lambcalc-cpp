#include "anf.h"
#include "utils.h"
#include <exception>
#include <sstream>

namespace lambcalc {
namespace anf {

static int counter = 0;
std::string fresh() { return std::string("tmp") + std::to_string(counter++); }

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
              .thenBranch = thenBranch.convert([&joinName](Value value) {
                return make(JumpExp{joinName, std::optional{std::move(value)}});
              }),
              .elseBranch = elseBranch.convert([&joinName](Value value) {
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

struct K;
struct K2;

struct K2 {
  enum { CONVERT = 0, LAM1, LAM2, APP1, BOP1, IF1, IF2, IF3, LENGTH } tag;
  union {
    struct {
    } convert;

    struct {
      K2 *k2;
      K *k;
      std::string v;
    } lam1;

    struct {
      K2 *k2;
      std::string f, v;
      std::unique_ptr<Exp> body;
    } lam2;

    struct {
      std::string r, f;
      Value x;
      K2 *k2;
    } app1;

    struct {
      std::string r;
      ast::Bop bop;
      Value x, y;
      K2 *k2;
    } bop1;

    struct {
      ast::Exp &t;
      ast::Exp &f;
      K2 *k2;
      std::string j, p;
      Value c;
    } if1;

    struct {
      ast::Exp &f;
      K2 *k2;
      std::string j, p;
      Value c;
      std::unique_ptr<Exp> rest;
    } if2;

    struct {
      std::unique_ptr<Exp> t;
      K2 *k2;
      std::string j, p;
      Value c;
      std::unique_ptr<Exp> rest;
    } if3;
  };

  void destroy();

  ~K2() { destroy(); }
};

struct K {
  enum { LAM1 = K2::LENGTH, APP1, APP2, BOP1, BOP2, IF1, IF2 } tag;
  union {
    struct {
    } lam1;

    struct {
      ast::Exp &x;
      K *k;
    } app1;

    struct {
      Value f;
      K *k;
    } app2;

    struct {
      ast::Exp &y;
      ast::Bop bop;
      K *k;
    } bop1;

    struct {
      Value x;
      ast::Bop bop;
      K *k;
    } bop2;

    struct {
      ast::Exp &t;
      ast::Exp &f;
      K *k;
    } if1;

    struct {
      std::string j;
    } if2;
  };

  void destroy();

  ~K() { destroy(); }
};

void K2::destroy() {
  switch (tag) {
  case LAM1:
    lam1.v.~basic_string();
    delete lam1.k2;
    delete lam1.k;
    break;
  case LAM2:
    lam2.f.~basic_string();
    lam2.v.~basic_string();
    delete lam2.k2;
    lam2.body.~unique_ptr();
    break;
  case APP1:
    app1.r.~basic_string();
    app1.f.~basic_string();
    delete app1.k2;
    break;
  case BOP1:
    bop1.r.~basic_string();
    delete bop1.k2;
    break;
  case IF1:
    delete if1.k2;
    if1.j.~basic_string();
    if1.p.~basic_string();
    break;
  case IF2:
    delete if2.k2;
    if2.rest.~unique_ptr();
    if2.j.~basic_string();
    if2.p.~basic_string();
    break;
  case IF3:
    if3.t.~unique_ptr();
    delete if3.k2;
    if3.j.~basic_string();
    if3.p.~basic_string();
    if3.rest.~unique_ptr();
    break;
  default:
    break;
  }
}

void K::destroy() {
  switch (tag) {
  case LAM1:
    break;
  case APP1:
    delete app1.k;
    break;
  case APP2:
    delete app2.k;
    break;
  case BOP1:
    delete bop1.k;
    break;
  case BOP2:
    delete bop2.k;
    break;
  case IF1:
    delete if1.k;
    break;
  case IF2:
    if2.j.~basic_string();
    break;
  }
}

std::unique_ptr<Exp> convertDefunc(ast::Exp &root) {
  // go's parameters
  ast::Exp *go_exp = &root;
  K2 *go_k2 = new K2{.tag = K2::CONVERT, .convert = {}};
  K *go_k = new K{.tag = K::LAM1, .lam1 = {}};

  // applyK2's parameters
  std::unique_ptr<Exp> k2_exp;

  // applyK's parameters
  Value k_value;
  K2 *k_k2 = nullptr;

  static const void *dispatch_table[] = {
      &&APPLY_K2_CONVERT, &&APPLY_K2_LAM1, &&APPLY_K2_LAM2, &&APPLY_K2_APP1,
      &&APPLY_K2_BOP1,    &&APPLY_K2_IF1,  &&APPLY_K2_IF2,  &&APPLY_K2_IF3,
      &&APPLY_K_LAM1,     &&APPLY_K_APP1,  &&APPLY_K_APP2,  &&APPLY_K_BOP1,
      &&APPLY_K_BOP2,     &&APPLY_K_IF1,   &&APPLY_K_IF2};

#define DISPATCH(tag) goto *dispatch_table[(tag)]

APPLY_K2_CONVERT:
  return k2_exp;

APPLY_K2_LAM1:
  auto f = fresh();

APPLY_K2_LAM2:

APPLY_K2_APP1:

APPLY_K2_BOP1:

APPLY_K2_IF1:

APPLY_K2_IF2:

APPLY_K2_IF3:

APPLY_K_LAM1:

APPLY_K_APP1:

APPLY_K_APP2:

APPLY_K_BOP1:

APPLY_K_BOP2:

APPLY_K_IF1:

APPLY_K_IF2:

  return nullptr;
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

std::string Exp::dump() {
  std::ostringstream out;
  out << *this;
  return out.str();
}

} // namespace ast
} // namespace lambcalc
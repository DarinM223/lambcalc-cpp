#include "anf.h"
#include "utils.h"
#include <exception>
#include <sstream>

namespace lambcalc {
namespace anf {

static int counter = 0;
std::string fresh() { return std::string("tmp") + std::to_string(counter++); }
void resetCounter() { counter = 0; }

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
    auto body = convert(*exp.body);
    auto name = fresh();
    return make(FunExp{
        .name = name,
        .params = {exp.param},
        .body = std::move(body),
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

struct KFrame;
struct K2Frame;

using K = std::vector<KFrame>;
using K2 = std::vector<K2Frame>;

struct K2_Lam1 {
  K k;
  std::string v;
};

struct K2_Lam2 {
  std::string f, v;
  std::unique_ptr<Exp> body;
};

struct K2_App1 {
  std::string r, f;
  Value x;
};

struct K2_Bop1 {
  std::string r;
  ast::Bop bop;
  Value x, y;
};

struct K2_If1 {
  ast::Exp &t;
  ast::Exp &f;
  std::string j, p;
  Value c;
};

struct K2_If2 {
  ast::Exp &f;
  std::string j, p;
  Value c;
  std::unique_ptr<Exp> rest;
};

struct K2_If3 {
  std::unique_ptr<Exp> t;
  std::string j, p;
  Value c;
  std::unique_ptr<Exp> rest;
};

struct K2Frame : public std::variant<K2_Lam1, K2_Lam2, K2_App1, K2_Bop1, K2_If1,
                                     K2_If2, K2_If3> {
  using variant::variant;
};

struct K_App1 {
  ast::Exp &x;
};

struct K_App2 {
  Value f;
};

struct K_Bop1 {
  ast::Exp &y;
  ast::Bop bop;
};

struct K_Bop2 {
  Value x;
  ast::Bop bop;
};

struct K_If1 {
  ast::Exp &t;
  ast::Exp &f;
};

struct K_If2 {
  std::string j;
};

struct KFrame
    : public std::variant<K_App1, K_App2, K_Bop1, K_Bop2, K_If1, K_If2> {
  using variant::variant;
};

std::unique_ptr<Exp> convertDefunc(ast::Exp &root) {
  // Parameters for apply_k2, apply_k, and go normalized.
  // If two parameters for different functions have the same type,
  // they can share the same variable because tail calls destroy the stack.
  ast::Exp *go_exp = &root;
  std::unique_ptr<Exp> k2_exp;
  K k;
  K2 k2;
  Value value;

  enum { APPLY_K2, APPLY_K, GO } dispatch = GO;

  while (true) {
    switch (dispatch) {
    case APPLY_K2: {
      if (k2.empty()) {
        return k2_exp;
      }
      auto frame = std::move(k2.back());
      k2.pop_back();
      std::visit(
          overloaded{
              [&](K2_Lam1 &frame) {
                auto f = fresh();
                k = std::move(frame.k);
                value = VarValue{f};
                k2.emplace_back(std::in_place_type<K2_Lam2>, f, frame.v,
                                std::move(k2_exp));
                dispatch = APPLY_K;
              },
              [&](K2_Lam2 &frame) {
                k2_exp = make(FunExp{.name = std::move(frame.f),
                                     .params = {std::move(frame.v)},
                                     .body = std::move(frame.body),
                                     .rest = std::move(k2_exp)});
              },
              [&](K2_App1 &frame) {
                k2_exp = make(AppExp{.name = std::move(frame.r),
                                     .funName = std::move(frame.f),
                                     .paramValues = {std::move(frame.x)},
                                     .rest = std::move(k2_exp)});
              },
              [&](K2_Bop1 &frame) {
                k2_exp = make(BopExp{.name = std::move(frame.r),
                                     .bop = frame.bop,
                                     .param1 = std::move(frame.x),
                                     .param2 = std::move(frame.y),
                                     .rest = std::move(k2_exp)});
              },
              [&](K2_If1 &frame) {
                go_exp = &frame.t;
                k2.emplace_back(std::in_place_type<K2_If2>, frame.f, frame.j,
                                std::move(frame.p), std::move(frame.c),
                                std::move(k2_exp));
                k.clear();
                k.emplace_back(std::in_place_type<K_If2>, frame.j);
                dispatch = GO;
              },
              [&](K2_If2 &frame) {
                go_exp = &frame.f;
                k2.emplace_back(std::in_place_type<K2_If3>, std::move(k2_exp),
                                frame.j, std::move(frame.p), std::move(frame.c),
                                std::move(frame.rest));
                k.clear();
                k.emplace_back(std::in_place_type<K_If2>, frame.j);
                dispatch = GO;
              },
              [&](K2_If3 &frame) {
                k2_exp = make(JoinExp{
                    .name = std::move(frame.j),
                    .slot = {std::move(frame.p)},
                    .body = std::move(frame.rest),
                    .rest = make(IfExp{.cond = std::move(frame.c),
                                       .thenBranch = std::move(frame.t),
                                       .elseBranch = std::move(k2_exp)})});
              },
          },
          frame);
      break;
    }
    case APPLY_K: {
      if (k.empty()) {
        k2_exp = make(HaltExp{value});
        dispatch = APPLY_K2;
        continue;
      }
      auto frame = std::move(k.back());
      k.pop_back();
      std::visit(
          overloaded{
              [&](K_App1 &frame) {
                go_exp = &frame.x;
                k.emplace_back(std::in_place_type<K_App2>, std::move(value));
                dispatch = GO;
              },
              [&](K_App2 &frame) {
                std::visit(overloaded{[&](VarValue &f) {
                                        auto r = fresh();
                                        k2.emplace_back(
                                            std::in_place_type<K2_App1>, r,
                                            f.var, std::move(value));
                                        value = VarValue{r};
                                      },
                                      [](auto &) {
                                        throw std::runtime_error(
                                            "must apply named value");
                                      }},
                           frame.f);
              },
              [&](K_Bop1 &frame) {
                go_exp = &frame.y;
                k.emplace_back(std::in_place_type<K_Bop2>, std::move(value),
                               frame.bop);
                dispatch = GO;
              },
              [&](K_Bop2 &frame) {
                auto r = fresh();
                k2.emplace_back(std::in_place_type<K2_Bop1>, r, frame.bop,
                                std::move(frame.x), std::move(value));
                value = VarValue{r};
              },
              [&](K_If1 &frame) {
                auto j = fresh();
                auto p = fresh();

                k2.emplace_back(std::in_place_type<K2_If1>, frame.t, frame.f,
                                std::move(j), p, std::move(value));
                value = VarValue{p};
              },
              [&](K_If2 &frame) {
                k2_exp = make(JumpExp{.joinName = std::move(frame.j),
                                      .slotValue = {std::move(value)}});
                dispatch = APPLY_K2;
              },
          },
          frame);
      break;
    }
    case GO:
      std::visit(
          overloaded{
              [&](ast::IntExp &exp) {
                value = IntValue{exp.value};
                dispatch = APPLY_K;
              },
              [&](ast::VarExp &exp) {
                value = VarValue{exp.name};
                dispatch = APPLY_K;
              },
              [&](ast::LamExp &exp) {
                go_exp = exp.body.get();
                K oldK;
                k.swap(oldK);
                k2.emplace_back(std::in_place_type<K2_Lam1>, std::move(oldK),
                                exp.param);
              },
              [&](ast::AppExp &exp) {
                go_exp = exp.fn.get();
                k.emplace_back(std::in_place_type<K_App1>, *exp.arg);
              },
              [&](ast::BopExp &exp) {
                go_exp = exp.arg1.get();
                k.emplace_back(std::in_place_type<K_Bop1>, *exp.arg2, exp.bop);
              },
              [&](ast::IfExp &exp) {
                go_exp = exp.cond.get();
                k.emplace_back(std::in_place_type<K_If1>, *exp.then, *exp.els);
              },
          },
          *go_exp);
      break;
    }
  }
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
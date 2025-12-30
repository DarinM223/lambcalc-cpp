#include "anf.h"
#include "utils.h"
#include "visitor.h"
#include <queue>
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
  std::unique_ptr<Exp> operator()(ast::LamExp<std::unique_ptr> &exp) {
    auto body = convert(*exp.body);
    auto name = fresh();
    return make(FunExp{
        .name = name,
        .params = {exp.param},
        .body = std::move(body),
        .rest = k(VarValue{name}),
    });
  }
  std::unique_ptr<Exp> operator()(ast::AppExp<std::unique_ptr> &exp) {
    return ast::convert(*exp.fn, [&arg = *exp.arg, &k = k](Value fnValue) {
      std::string fnName =
          std::visit(StringValueVisitor<"function">{}, std::move(fnValue));
      return ast::convert(arg,
                          [fnName = std::move(fnName), &k = k](Value argValue) {
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
  std::unique_ptr<Exp> operator()(ast::BopExp<std::unique_ptr> &exp) {
    return ast::convert(
        *exp.arg1, [bop = exp.bop, &arg2 = *exp.arg2, &k = k](Value arg1Value) {
          return ast::convert(arg2, [bop, arg1Value = std::move(arg1Value),
                                     &k = k](Value arg2Value) {
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
  std::unique_ptr<Exp> operator()(ast::IfExp<std::unique_ptr> &exp) {
    return ast::convert(*exp.cond, [&thenBranch = *exp.then,
                                    &elseBranch = *exp.els,
                                    &k = k](Value condValue) {
      auto joinName = fresh();
      auto slot = fresh();
      return make(JoinExp{
          .name = joinName,
          .slot = std::optional{slot},
          .body = k(VarValue{slot}),
          .rest = make(IfExp{
              .cond = condValue,
              .thenBranch = ast::convert(
                  thenBranch,
                  [&joinName](Value value) {
                    return make(
                        JumpExp{joinName, std::optional{std::move(value)}});
                  }),
              .elseBranch = ast::convert(elseBranch, [&joinName](Value value) {
                return make(JumpExp{joinName, std::optional{std::move(value)}});
              })})});
    });
  }
};

std::unique_ptr<Exp> make(Exp &&exp) {
  return std::make_unique<Exp>(std::move(exp));
}

std::unique_ptr<Exp> convert(ast::Exp<> &exp) {
  return ast::convert(exp, [](Value value) { return make(HaltExp{value}); });
}

struct DestructorVisitor
    : WorklistVisitor<DefaultVisitor, DestructorTask<Exp>, std::queue> {};

Exp::~Exp() {
  DestructorVisitor visitor;
  auto &queue = visitor.getWorklist();
  std::visit(visitor, *this);
  while (!queue.empty()) {
    auto task = std::move(queue.front());
    queue.pop();

    if (task.exp != nullptr) {
      std::visit(visitor, *task.exp);
    }
  }
}

template <template <class> class Ptr> struct KFrame;
template <template <class> class Ptr> struct K2Frame;
template <template <class> class Ptr> using K = std::vector<KFrame<Ptr>>;
template <template <class> class Ptr> using K2 = std::vector<K2Frame<Ptr>>;
template <template <class> class Ptr> struct K2_Lam1 {
  K<Ptr> k;
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

template <template <class> class Ptr> struct K2_If1 {
  ast::Exp<Ptr> &t;
  ast::Exp<Ptr> &f;
  std::string j, p;
  Value c;
};

template <template <class> class Ptr> struct K2_If2 {
  ast::Exp<Ptr> &f;
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

template <template <class> class Ptr>
struct K2Frame : public std::variant<K2_Lam1<Ptr>, K2_Lam2, K2_App1, K2_Bop1,
                                     K2_If1<Ptr>, K2_If2<Ptr>, K2_If3> {
  using std::variant<K2_Lam1<Ptr>, K2_Lam2, K2_App1, K2_Bop1, K2_If1<Ptr>,
                     K2_If2<Ptr>, K2_If3>::variant;
};

template <template <class> class Ptr> struct K_App1 {
  ast::Exp<Ptr> &x;
};

struct K_App2 {
  Value f;
};

template <template <class> class Ptr> struct K_Bop1 {
  ast::Exp<Ptr> &y;
  ast::Bop bop;
};

struct K_Bop2 {
  Value x;
  ast::Bop bop;
};

template <template <class> class Ptr> struct K_If1 {
  ast::Exp<Ptr> &t;
  ast::Exp<Ptr> &f;
};

struct K_If2 {
  std::string j;
};

template <template <class> class Ptr>
struct KFrame : public std::variant<K_App1<Ptr>, K_App2, K_Bop1<Ptr>, K_Bop2,
                                    K_If1<Ptr>, K_If2> {
  using std::variant<K_App1<Ptr>, K_App2, K_Bop1<Ptr>, K_Bop2, K_If1<Ptr>,
                     K_If2>::variant;
};

template <template <class> class Ptr>
std::unique_ptr<Exp> convertDefunc(ast::Exp<Ptr> &root) {
  // Parameters for apply_k2, apply_k, and go normalized.
  // If two parameters for different functions have the same type,
  // they can share the same variable because tail calls destroy the stack.
  ast::Exp<Ptr> *go_exp = &root;
  std::unique_ptr<Exp> k2_exp;
  K<Ptr> k;
  K2<Ptr> k2;
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
              [&](K2_Lam1<Ptr> &frame) {
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
              [&](K2_If1<Ptr> &frame) {
                go_exp = &frame.t;
                k2.emplace_back(std::in_place_type<K2_If2<Ptr>>, frame.f,
                                frame.j, std::move(frame.p), std::move(frame.c),
                                std::move(k2_exp));
                k.clear();
                k.emplace_back(std::in_place_type<K_If2>, frame.j);
                dispatch = GO;
              },
              [&](K2_If2<Ptr> &frame) {
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
              [&](K_App1<Ptr> &frame) {
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
              [&](K_Bop1<Ptr> &frame) {
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
              [&](K_If1<Ptr> &frame) {
                auto j = fresh();
                auto p = fresh();

                k2.emplace_back(std::in_place_type<K2_If1<Ptr>>, frame.t,
                                frame.f, std::move(j), p, std::move(value));
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
      std::visit(overloaded{
                     [&](ast::IntExp &exp) {
                       value = IntValue{exp.value};
                       dispatch = APPLY_K;
                     },
                     [&](ast::VarExp &exp) {
                       value = VarValue{exp.name};
                       dispatch = APPLY_K;
                     },
                     [&](ast::LamExp<Ptr> &exp) {
                       go_exp = &*exp.body;
                       K<Ptr> oldK;
                       k.swap(oldK);
                       k2.emplace_back(std::in_place_type<K2_Lam1<Ptr>>,
                                       std::move(oldK), exp.param);
                     },
                     [&](ast::AppExp<Ptr> &exp) {
                       go_exp = &*exp.fn;
                       k.emplace_back(std::in_place_type<K_App1<Ptr>>,
                                      *exp.arg);
                     },
                     [&](ast::BopExp<Ptr> &exp) {
                       go_exp = &*exp.arg1;
                       k.emplace_back(std::in_place_type<K_Bop1<Ptr>>,
                                      *exp.arg2, exp.bop);
                     },
                     [&](ast::IfExp<Ptr> &exp) {
                       go_exp = &*exp.cond;
                       k.emplace_back(std::in_place_type<K_If1<Ptr>>, *exp.then,
                                      *exp.els);
                     },
                 },
                 *go_exp);
      break;
    }
  }
  return nullptr;
}

template std::unique_ptr<Exp> convertDefunc(ast::Exp<std::unique_ptr> &root);
template std::unique_ptr<Exp> convertDefunc(ast::Exp<raw_ptr> &root);

std::string Exp::dump() {
  std::ostringstream out;
  out << *this;
  return out.str();
}

} // namespace anf

namespace ast {

std::unique_ptr<anf::Exp> convert(Exp<> &exp, anf::Cont k) {
  return std::visit(anf::AnfConvertVisitor{.k = std::move(k)}, exp);
}

} // namespace ast
} // namespace lambcalc
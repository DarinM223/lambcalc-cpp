#include "anf.h"
#include "ast.h"
#include "utils.h"
#include <memory>
#include <utility>
#include <vector>

namespace lambcalc {
namespace anf {

enum K_Tag { K_LAM1 = 0, K_APP1, K_APP2, K_BOP1, K_BOP2, K_IF1, K_IF2 };
enum K2_Tag {
  K2_CONVERT = 0,
  K2_LAM1,
  K2_LAM2,
  K2_APP1,
  K2_BOP1,
  K2_IF1,
  K2_IF2,
  K2_IF3
};

template <template <class> class Ptr> struct K;
template <template <class> class Ptr> struct K2;

template <template <class> class Ptr> struct K_App1 {
  ast::Exp<Ptr> *x;
};

struct K_App2 {
  Value f;
};

template <template <class> class Ptr> struct K_Bop1 {
  ast::Exp<Ptr> *y;
  ast::Bop bop;
};

struct K_Bop2 {
  Value x;
  ast::Bop bop;
};

template <template <class> class Ptr> struct K_If1 {
  ast::Exp<Ptr> *t, *f;
};

struct K_If2 {
  Symbol j;
};

template <template <class> class Ptr> struct K {
  K_Tag tag;
  union {
    K_App1<Ptr> app1;
    K_App2 app2;
    K_Bop1<Ptr> bop1;
    K_Bop2 bop2;
    K_If1<Ptr> if1;
    K_If2 if2;
  };
};

struct K2_Lam1 {
  Symbol v;
};

struct K2_Lam2 {
  Symbol f, v;
  Exp *body;
};

struct K2_App1 {
  Symbol r, f;
  Value x;
};

struct K2_Bop1 {
  Symbol r;
  ast::Bop bop;
  Value x, y;
};

template <template <class> class Ptr> struct K2_If1 {
  ast::Exp<Ptr> *t, *f;
  Symbol j, p;
  Value c;
};

template <template <class> class Ptr> struct K2_If2 {
  ast::Exp<Ptr> *f;
  Symbol j, p;
  Value c;
  Exp *rest;
};

struct K2_If3 {
  Exp *t;
  Symbol j, p;
  Value c;
  Exp *rest;
};

template <template <class> class Ptr> struct K2 {
  K2_Tag tag;
  union {
    K2_Lam1 lam1;
    K2_Lam2 lam2;
    K2_App1 app1;
    K2_Bop1 bop1;
    K2_If1<Ptr> if1;
    K2_If2<Ptr> if2;
    K2_If3 if3;
  };
};

// NOTE: Hopefully this elides the runtime check.
template <typename T, template <class> class Ptr>
const T *getVariant(ast::Exp<Ptr> *ptr) {
  return std::visit(overloaded{[](const T &exp) { return &exp; },
                               [](const auto &) {
                                 std::unreachable();
                                 const T *result = nullptr;
                                 return result;
                               }},
                    *ptr);
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

template <template <class> class Ptr>
std::unique_ptr<Exp> convertComputedGoto(SymbolTable &table,
                                         ast::Exp<Ptr> &root) {
  // Parameters for apply_k2, apply_k, and go normalized.
  // If two parameters for different functions have the same type,
  // they can share the same variable because tail calls destroy the stack.
  ast::Exp<Ptr> *go_exp = &root;
  std::unique_ptr<Exp> k2_exp;
  std::vector<std::vector<K<Ptr>>> storedLamK;
  std::vector<K<Ptr>> k;
  k.push_back({K_LAM1, {}});
  std::vector<K2<Ptr>> k2;
  k2.push_back({K2_CONVERT, {}});
  Value value;

  static void *apply_k2_table[] = {
      &&apply_k2_convert, &&apply_k2_lam1, &&apply_k2_lam2, &&apply_k2_app1,
      &&apply_k2_bop1,    &&apply_k2_if1,  &&apply_k2_if2,  &&apply_k2_if3};

  static void *apply_k_table[] = {
      &&apply_k_lam1, &&apply_k_app1, &&apply_k_app2, &&apply_k_bop1,
      &&apply_k_bop2, &&apply_k_if1,  &&apply_k_if2};

  static void *go_table[] = {&&go_int_exp, &&go_var_exp, &&go_lam_exp,
                             &&go_app_exp, &&go_bop_exp, &&go_if_exp};

  goto *go_table[go_exp->index()];

  while (true) {
  apply_k2_convert:
    return k2_exp;
  apply_k2_lam1: {
    auto frame = k2.back().lam1;
    auto f = fresh(table);

    k = std::move(storedLamK.back());
    storedLamK.pop_back();

    value = VarValue{f};
    k2.back() = K2<Ptr>{
        .tag = K2_LAM2,
        .lam2 = {.f = f, .v = frame.v, .body = std::move(k2_exp).release()}};
    goto *apply_k_table[k.back().tag];
  }
  apply_k2_lam2: {
    auto frame = k2.back().lam2;
    k2_exp = make(FunExp{.name = frame.f,
                         .params = {frame.v},
                         .body = std::unique_ptr<Exp>(frame.body),
                         .rest = std::move(k2_exp)});
    k2.pop_back();
    goto *apply_k2_table[k2.back().tag];
  }
  apply_k2_app1: {
    auto frame = k2.back().app1;
    k2_exp = make(AppExp{.name = frame.r,
                         .funName = frame.f,
                         .paramValues = {std::move(frame.x)},
                         .rest = std::move(k2_exp)});
    k2.pop_back();
    goto *apply_k2_table[k2.back().tag];
  }
  apply_k2_bop1: {
    auto frame = k2.back().bop1;
    k2_exp = make(BopExp{.name = frame.r,
                         .bop = frame.bop,
                         .param1 = std::move(frame.x),
                         .param2 = std::move(frame.y),
                         .rest = std::move(k2_exp)});
    k2.pop_back();
    goto *apply_k2_table[k2.back().tag];
  }
  apply_k2_if1: {
    auto frame = k2.back().if1;
    go_exp = frame.t;

    k.clear();
    k.push_back({.tag = K_IF2, .if2 = {.j = frame.j}});

    k2.back() = K2<Ptr>{.tag = K2_IF2,
                        .if2 = {.f = frame.f,
                                .j = frame.j,
                                .p = frame.p,
                                .c = std::move(frame.c),
                                .rest = std::move(k2_exp).release()}};

    goto *go_table[go_exp->index()];
  }
  apply_k2_if2: {
    auto frame = k2.back().if2;
    go_exp = frame.f;

    k.clear();
    k.push_back({.tag = K_IF2, .if2 = {.j = frame.j}});

    k2.back() = K2<Ptr>{.tag = K2_IF3,
                        .if3 = {.t = std::move(k2_exp).release(),
                                .j = frame.j,
                                .p = frame.p,
                                .c = std::move(frame.c),
                                .rest = frame.rest}};

    goto *go_table[go_exp->index()];
  }
  apply_k2_if3: {
    auto frame = k2.back().if3;
    k2_exp = make(
        JoinExp{.name = frame.j,
                .slot = {frame.p},
                .body = std::unique_ptr<Exp>(frame.rest),
                .rest = make(IfExp{.cond = std::move(frame.c),
                                   .thenBranch = std::unique_ptr<Exp>(frame.t),
                                   .elseBranch = std::move(k2_exp)})});
    k2.pop_back();
    goto *apply_k2_table[k2.back().tag];
  }

  apply_k_lam1:
    k2_exp = make(HaltExp{value});
    goto *apply_k2_table[k2.back().tag];
  apply_k_app1: {
    auto frame = k.back().app1;
    go_exp = frame.x;

    k.back() = K<Ptr>{.tag = K_APP2, .app2 = {.f = std::move(value)}};

    goto *go_table[go_exp->index()];
  }
  apply_k_app2: {
    auto frame = k.back().app2;

    std::visit(
        overloaded{
            [&](VarValue &f) {
              auto r = fresh(table);
              k2.push_back(
                  {.tag = K2_APP1,
                   .app1 = {.r = r, .f = f.var, .x = std::move(value)}});
              value = VarValue{r};
            },
            [](auto &) { throw std::runtime_error("must apply named value"); }},
        frame.f);
    k.pop_back();
    goto *apply_k_table[k.back().tag];
  }
  apply_k_bop1: {
    auto frame = k.back().bop1;
    go_exp = frame.y;

    k.back() = K<Ptr>{.tag = K_BOP2,
                      .bop2 = {.x = std::move(value), .bop = frame.bop}};

    goto *go_table[go_exp->index()];
  }
  apply_k_bop2: {
    auto frame = k.back().bop2;
    auto r = fresh(table);

    k2.push_back({.tag = K2_BOP1,
                  .bop1 = {.r = r,
                           .bop = frame.bop,
                           .x = std::move(frame.x),
                           .y = std::move(value)}});

    value = VarValue{r};
    k.pop_back();
    goto *apply_k_table[k.back().tag];
  }
  apply_k_if1: {
    auto frame = k.back().if1;
    auto j = fresh(table);
    auto p = fresh(table);

    k2.push_back({.tag = K2_IF1,
                  .if1 = {.t = frame.t,
                          .f = frame.f,
                          .j = j,
                          .p = p,
                          .c = std::move(value)}});

    value = VarValue{p};
    k.pop_back();
    goto *apply_k_table[k.back().tag];
  }
  apply_k_if2: {
    auto frame = k.back().if2;
    k2_exp =
        make(JumpExp{.joinName = frame.j, .slotValue = {std::move(value)}});
    goto *apply_k2_table[k2.back().tag];
  }
  go_int_exp: {
    auto exp = getVariant<ast::IntExp>(go_exp);
    value = IntValue{exp->value};
    goto *apply_k_table[k.back().tag];
  }
  go_var_exp: {
    auto exp = getVariant<ast::VarExp>(go_exp);
    value = VarValue{exp->name};
    goto *apply_k_table[k.back().tag];
  }
  go_lam_exp: {
    auto exp = getVariant<ast::LamExp<Ptr>>(go_exp);
    go_exp = &*exp->body;
    {
      std::vector<K<Ptr>> oldK;
      k.swap(oldK);
      k.push_back({K_LAM1, {}});
      storedLamK.emplace_back(std::move(oldK));
    }

    k2.push_back({.tag = K2_LAM1, .lam1 = {.v = exp->param}});
    goto *go_table[go_exp->index()];
  }
  go_app_exp: {
    auto exp = getVariant<ast::AppExp<Ptr>>(go_exp);
    go_exp = &*exp->fn;
    k.push_back({.tag = K_APP1, .app1 = {.x = &*exp->arg}});
    goto *go_table[go_exp->index()];
  }
  go_bop_exp: {
    auto exp = getVariant<ast::BopExp<Ptr>>(go_exp);
    go_exp = &*exp->arg1;
    k.push_back({.tag = K_BOP1, .bop1 = {.y = &*exp->arg2, .bop = exp->bop}});
    goto *go_table[go_exp->index()];
  }
  go_if_exp: {
    auto exp = getVariant<ast::IfExp<Ptr>>(go_exp);
    go_exp = &*exp->cond;
    k.push_back({.tag = K_IF1, .if1 = {.t = &*exp->then, .f = &*exp->els}});
    goto *go_table[go_exp->index()];
  }
  }
  return nullptr;
}

template std::unique_ptr<Exp>
convertComputedGoto(SymbolTable &table, ast::Exp<std::unique_ptr> &root);
template std::unique_ptr<Exp> convertComputedGoto(SymbolTable &table,
                                                  ast::Exp<raw_ptr> &root);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

} // namespace anf
} // namespace lambcalc
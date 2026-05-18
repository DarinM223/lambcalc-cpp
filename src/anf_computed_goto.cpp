#include "anf.h"
#include "arena.h"
#include "ast.h"
#include "utils.h"
#include <memory>
#include <utility>

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
  K<Ptr> *k;
};

template <template <class> class Ptr> struct K_App2 {
  Value f;
  K<Ptr> *k;
};

template <template <class> class Ptr> struct K_Bop1 {
  ast::Exp<Ptr> *y;
  ast::Bop bop;
  K<Ptr> *k;
};

template <template <class> class Ptr> struct K_Bop2 {
  Value x;
  ast::Bop bop;
  K<Ptr> *k;
};

template <template <class> class Ptr> struct K_If1 {
  ast::Exp<Ptr> *t, *f;
  K<Ptr> *k;
};

struct K_If2 {
  Symbol j;
};

template <template <class> class Ptr> struct K {
  K_Tag tag;
  union {
    K_App1<Ptr> app1;
    K_App2<Ptr> app2;
    K_Bop1<Ptr> bop1;
    K_Bop2<Ptr> bop2;
    K_If1<Ptr> if1;
    K_If2 if2;
  };
};

template <template <class> class Ptr> struct K2_Lam1 {
  K2<Ptr> *k2;
  K<Ptr> *k;
  Symbol v;
};

template <template <class> class Ptr> struct K2_Lam2 {
  K2<Ptr> *k2;
  Symbol f, v;
  Exp *body;
};

template <template <class> class Ptr> struct K2_App1 {
  Symbol r, f;
  Value x;
  K2<Ptr> *k2;
};

template <template <class> class Ptr> struct K2_Bop1 {
  Symbol r;
  ast::Bop bop;
  Value x, y;
  K2<Ptr> *k2;
};

template <template <class> class Ptr> struct K2_If1 {
  ast::Exp<Ptr> *t, *f;
  K2<Ptr> *k2;
  Symbol j, p;
  Value c;
};

template <template <class> class Ptr> struct K2_If2 {
  ast::Exp<Ptr> *f;
  K2<Ptr> *k2;
  Symbol j, p;
  Value c;
  Exp *rest;
};

template <template <class> class Ptr> struct K2_If3 {
  Exp *t;
  K2<Ptr> *k2;
  Symbol j, p;
  Value c;
  Exp *rest;
};

template <template <class> class Ptr> struct K2 {
  K2_Tag tag;
  union {
    K2_Lam1<Ptr> lam1;
    K2_Lam2<Ptr> lam2;
    K2_App1<Ptr> app1;
    K2_Bop1<Ptr> bop1;
    K2_If1<Ptr> if1;
    K2_If2<Ptr> if2;
    K2_If3<Ptr> if3;
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

template <template <class> class Ptr>
std::unique_ptr<Exp> convertComputedGoto(SymbolTable &table,
                                         ast::Exp<Ptr> &root) {
  arena::LinkedAllocator allocator(1 << 28);
  // Parameters for apply_k2, apply_k, and go normalized.
  // If two parameters for different functions have the same type,
  // they can share the same variable because tail calls destroy the stack.
  ast::Exp<Ptr> *go_exp = &root;
  std::unique_ptr<Exp> k2_exp;
  auto k = allocator.allocate<K<Ptr>>();
  k->tag = K_LAM1;
  auto k2 = allocator.allocate<K2<Ptr>>();
  k2->tag = K2_CONVERT;
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
    auto frame = k2->lam1;
    auto f = fresh(table);
    k = frame.k;
    value = VarValue{f};
    auto newK2 = allocator.allocate<K2<Ptr>>();
    newK2->tag = K2_LAM2;
    newK2->lam2.f = f;
    newK2->lam2.v = frame.v;
    {
      std::unique_ptr<Exp> body = std::move(k2_exp);
      newK2->lam2.body = body.release();
    }
    newK2->lam2.k2 = frame.k2;
    k2 = newK2;
    goto *apply_k_table[k->tag];
  }
  apply_k2_lam2: {
    auto frame = k2->lam2;
    k2_exp = make(FunExp{.name = frame.f,
                         .params = {frame.v},
                         .body = std::unique_ptr<Exp>(frame.body),
                         .rest = std::move(k2_exp)});
    k2 = frame.k2;
    goto *apply_k2_table[k2->tag];
  }
  apply_k2_app1: {
    auto frame = k2->app1;
    k2_exp = make(AppExp{.name = frame.r,
                         .funName = frame.f,
                         .paramValues = {std::move(frame.x)},
                         .rest = std::move(k2_exp)});
    k2 = frame.k2;
    goto *apply_k2_table[k2->tag];
  }
  apply_k2_bop1: {
    auto frame = k2->bop1;
    k2_exp = make(BopExp{.name = frame.r,
                         .bop = frame.bop,
                         .param1 = std::move(frame.x),
                         .param2 = std::move(frame.y),
                         .rest = std::move(k2_exp)});
    k2 = frame.k2;
    goto *apply_k2_table[k2->tag];
  }
  apply_k2_if1: {
    auto frame = k2->if1;
    go_exp = frame.t;

    auto newK2 = allocator.allocate<K2<Ptr>>();
    newK2->tag = K2_IF2;
    newK2->if2.f = frame.f;
    newK2->if2.j = frame.j;
    newK2->if2.p = frame.p;
    newK2->if2.c = std::move(frame.c);
    {
      std::unique_ptr<Exp> rest = std::move(k2_exp);
      newK2->if2.rest = rest.release();
    }
    newK2->if2.k2 = frame.k2;
    k2 = newK2;

    k = allocator.allocate<K<Ptr>>();
    k->tag = K_IF2;
    k->if2.j = frame.j;

    goto *go_table[go_exp->index()];
  }
  apply_k2_if2: {
    auto frame = k2->if2;
    go_exp = frame.f;

    auto newK2 = allocator.allocate<K2<Ptr>>();
    newK2->tag = K2_IF3;
    {
      std::unique_ptr<Exp> t = std::move(k2_exp);
      newK2->if3.t = t.release();
    }
    newK2->if3.j = frame.j;
    newK2->if3.p = frame.p;
    newK2->if3.c = std::move(frame.c);
    newK2->if3.rest = frame.rest;
    newK2->if3.k2 = frame.k2;
    k2 = newK2;

    k = allocator.allocate<K<Ptr>>();
    k->tag = K_IF2;
    k->if2.j = frame.j;

    goto *go_table[go_exp->index()];
  }
  apply_k2_if3: {
    auto frame = k2->if3;
    k2_exp = make(
        JoinExp{.name = frame.j,
                .slot = {frame.p},
                .body = std::unique_ptr<Exp>(frame.rest),
                .rest = make(IfExp{.cond = std::move(frame.c),
                                   .thenBranch = std::unique_ptr<Exp>(frame.t),
                                   .elseBranch = std::move(k2_exp)})});
    k2 = frame.k2;
    goto *apply_k2_table[k2->tag];
  }

  apply_k_lam1:
    k2_exp = make(HaltExp{value});
    goto *apply_k2_table[k2->tag];
  apply_k_app1: {
    auto frame = k->app1;
    go_exp = frame.x;

    auto newK = allocator.allocate<K<Ptr>>();
    newK->tag = K_APP2;
    newK->app2.f = std::move(value);
    newK->app2.k = frame.k;
    k = newK;

    goto *go_table[go_exp->index()];
  }
  apply_k_app2: {
    auto frame = k->app2;

    std::visit(overloaded{[&](VarValue &f) {
                            auto r = fresh(table);
                            auto newK2 = allocator.allocate<K2<Ptr>>();
                            newK2->tag = K2_APP1;
                            newK2->app1.r = r;
                            newK2->app1.f = f.var;
                            newK2->app1.x = std::move(value);
                            newK2->app1.k2 = k2;
                            k2 = newK2;
                            value = VarValue{r};
                          },
                          [](auto &) {
                            throw std::runtime_error("must apply named value");
                          }},
               frame.f);
    k = frame.k;
    goto *apply_k_table[k->tag];
  }
  apply_k_bop1: {
    auto frame = k->bop1;
    go_exp = frame.y;

    auto newK = allocator.allocate<K<Ptr>>();
    newK->tag = K_BOP2;
    newK->bop2.x = std::move(value);
    newK->bop2.bop = frame.bop;
    newK->bop2.k = frame.k;
    k = newK;

    goto *go_table[go_exp->index()];
  }
  apply_k_bop2: {
    auto frame = k->bop2;
    auto r = fresh(table);

    auto newK2 = allocator.allocate<K2<Ptr>>();
    newK2->tag = K2_BOP1;
    newK2->bop1.r = r;
    newK2->bop1.bop = frame.bop;
    newK2->bop1.x = std::move(frame.x);
    newK2->bop1.y = std::move(value);
    newK2->bop1.k2 = k2;
    k2 = newK2;

    value = VarValue{r};
    k = frame.k;
    goto *apply_k_table[k->tag];
  }
  apply_k_if1: {
    auto frame = k->if1;
    auto j = fresh(table);
    auto p = fresh(table);

    auto newK2 = allocator.allocate<K2<Ptr>>();
    newK2->tag = K2_IF1;
    newK2->if1.t = frame.t;
    newK2->if1.f = frame.f;
    newK2->if1.j = j;
    newK2->if1.p = p;
    newK2->if1.c = std::move(value);
    newK2->if1.k2 = k2;
    k2 = newK2;

    value = VarValue{p};
    k = frame.k;
    goto *apply_k_table[k->tag];
  }
  apply_k_if2: {
    auto frame = k->if2;
    k2_exp =
        make(JumpExp{.joinName = frame.j, .slotValue = {std::move(value)}});
    goto *apply_k2_table[k2->tag];
  }
  go_int_exp: {
    auto exp = getVariant<ast::IntExp>(go_exp);
    value = IntValue{exp->value};
    goto *apply_k_table[k->tag];
  }
  go_var_exp: {
    auto exp = getVariant<ast::VarExp>(go_exp);
    value = VarValue{exp->name};
    goto *apply_k_table[k->tag];
  }
  go_lam_exp: {
    auto exp = getVariant<ast::LamExp<Ptr>>(go_exp);
    go_exp = &*exp->body;
    auto oldK = k;
    k = allocator.allocate<K<Ptr>>();
    k->tag = K_LAM1;

    auto newK2 = allocator.allocate<K2<Ptr>>();
    newK2->tag = K2_LAM1;
    newK2->lam1.k = oldK;
    newK2->lam1.v = exp->param;
    newK2->lam1.k2 = k2;
    k2 = newK2;
    goto *go_table[go_exp->index()];
  }
  go_app_exp: {
    auto exp = getVariant<ast::AppExp<Ptr>>(go_exp);
    go_exp = &*exp->fn;
    auto newK = allocator.allocate<K<Ptr>>();
    newK->tag = K_APP1;
    newK->app1.x = &*exp->arg;
    newK->app1.k = k;
    k = newK;
    goto *go_table[go_exp->index()];
  }
  go_bop_exp: {
    auto exp = getVariant<ast::BopExp<Ptr>>(go_exp);
    go_exp = &*exp->arg1;
    auto newK = allocator.allocate<K<Ptr>>();
    newK->tag = K_BOP1;
    newK->bop1.y = &*exp->arg2;
    newK->bop1.bop = exp->bop;
    newK->bop1.k = k;
    k = newK;
    goto *go_table[go_exp->index()];
  }
  go_if_exp: {
    auto exp = getVariant<ast::IfExp<Ptr>>(go_exp);
    go_exp = &*exp->cond;
    auto newK = allocator.allocate<K<Ptr>>();
    newK->tag = K_IF1;
    newK->if1.t = &*exp->then;
    newK->if1.f = &*exp->els;
    newK->if1.k = k;
    k = newK;
    goto *go_table[go_exp->index()];
  }
  }
  return nullptr;
}

template std::unique_ptr<Exp>
convertComputedGoto(SymbolTable &table, ast::Exp<std::unique_ptr> &root);
template std::unique_ptr<Exp> convertComputedGoto(SymbolTable &table,
                                                  ast::Exp<raw_ptr> &root);

#pragma GCC diagnostic pop
#endif

} // namespace anf
} // namespace lambcalc
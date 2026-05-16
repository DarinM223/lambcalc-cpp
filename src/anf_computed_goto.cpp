#include "anf.h"
#include "arena.h"
#include "ast.h"
#include "utils.h"
#include <memory>
#include <utility>

namespace lambcalc {
namespace anf {

static int counter = 0;
Symbol fresh(SymbolTable &table) {
  return table.lookup(std::string("tmp") + std::to_string(counter++));
}
void resetCounter() { counter = 0; }

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

struct K;
struct K2;

struct K_App1 {
  ast::Exp<raw_ptr> *x;
  K *k;
};

struct K_App2 {
  Value f;
  K *k;
};

struct K_Bop1 {
  ast::Exp<raw_ptr> *y;
  ast::Bop bop;
  K *k;
};

struct K_Bop2 {
  Value y;
  ast::Bop bop;
  K *k;
};

struct K_If1 {
  ast::Exp<raw_ptr> *t, *f;
  K *k;
};

struct K_If2 {
  Symbol j;
};

struct K {
  K_Tag tag;
  union {
    K_App1 app1;
    K_App2 app2;
    K_Bop1 bop1;
    K_Bop2 bop2;
    K_If1 if1;
    K_If2 if2;
  };
};

struct K2_Lam1 {
  K2 *k2;
  K *k;
  Symbol v;
};

struct K2_Lam2 {
  K2 *k2;
  Symbol f, v;
  Exp *body;
};

struct K2_App1 {
  Symbol r, f;
  Value x;
  K2 *k2;
};

struct K2_Bop1 {
  Symbol r;
  ast::Bop bop;
  Value x, y;
  K2 *k2;
};

struct K2_If1 {
  ast::Exp<raw_ptr> *t, *f;
  K2 *k2;
  Symbol j, p;
  Value c;
};

struct K2_If2 {
  ast::Exp<raw_ptr> *f;
  K2 *k2;
  Symbol j, p;
  Value c;
  Exp *rest;
};

struct K2_If3 {
  Exp *t;
  K2 *k2;
  Symbol j, p;
  Value c;
  Exp *rest;
};

struct K2 {
  K2_Tag tag;
  union {
    K2_Lam1 lam1;
    K2_Lam2 lam2;
    K2_App1 app1;
    K2_Bop1 bop1;
    K2_If1 if1;
    K2_If2 if2;
    K2_If3 if3;
  };
};

// NOTE: Hopefully this elides the runtime check.
template <typename T> const T *getVariant(ast::Exp<raw_ptr> *ptr) {
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

std::unique_ptr<Exp> convertDefunc(SymbolTable &table,
                                   ast::Exp<raw_ptr> &root) {
  arena::LinkedAllocator allocator(1 << 28);
  // Parameters for apply_k2, apply_k, and go normalized.
  // If two parameters for different functions have the same type,
  // they can share the same variable because tail calls destroy the stack.
  ast::Exp<raw_ptr> *go_exp = &root;
  std::unique_ptr<Exp> k2_exp;
  K *k = allocator.allocate<K>();
  k->tag = K_LAM1;
  K2 *k2 = allocator.allocate<K2>();
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
    K2 *newK2 = allocator.allocate<K2>();
    newK2->tag = K2_LAM2;
    newK2->lam2.k2 = frame.k2;
    newK2->lam2.f = f;
    newK2->lam2.v = frame.v;
    newK2->lam2.body = &*k2_exp;
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
  apply_k2_bop1:
  apply_k2_if1:
  apply_k2_if2:
  apply_k2_if3:

  apply_k_lam1:
  apply_k_app1:
  apply_k_app2:
  apply_k_bop1:
  apply_k_bop2:
  apply_k_if1:
  apply_k_if2:
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
    auto exp = getVariant<ast::LamExp<raw_ptr>>(go_exp);
    go_exp = exp->body;
    K *oldK = k;
    k = allocator.allocate<K>();
    k->tag = K_LAM1;

    K2 *newK2 = allocator.allocate<K2>();
    newK2->tag = K2_LAM1;
    newK2->lam1.k = oldK;
    newK2->lam1.k2 = k2;
    newK2->lam1.v = exp->param;
    k2 = newK2;
    goto *go_table[go_exp->index()];
  }
  go_app_exp: {
    auto exp = getVariant<ast::AppExp<raw_ptr>>(go_exp);
    go_exp = exp->fn;
    K *newK = allocator.allocate<K>();
    newK->tag = K_APP1;
    newK->app1.k = k;
    newK->app1.x = exp->arg;
    k = newK;
    goto *go_table[go_exp->index()];
  }
  go_bop_exp: {
    auto exp = getVariant<ast::BopExp<raw_ptr>>(go_exp);
    go_exp = exp->arg1;
    K *newK = allocator.allocate<K>();
    newK->tag = K_BOP1;
    newK->bop1.k = k;
    newK->bop1.y = exp->arg2;
    newK->bop1.bop = exp->bop;
    k = newK;
    goto *go_table[go_exp->index()];
  }
  go_if_exp: {
    auto exp = getVariant<ast::IfExp<raw_ptr>>(go_exp);
    go_exp = exp->cond;
    K *newK = allocator.allocate<K>();
    k->tag = K_IF1;
    k->if1.k = k;
    k->if1.t = exp->then;
    k->if1.f = exp->els;
    k = newK;
    goto *go_table[go_exp->index()];
  }
  }
  return nullptr;
}

#pragma GCC diagnostic pop
#endif

} // namespace anf
} // namespace lambcalc
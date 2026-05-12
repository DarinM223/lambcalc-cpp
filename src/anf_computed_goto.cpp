#include "anf.h"
#include "ast.h"
#include "utils.h"

namespace lambcalc {

enum K_Tag { K_LAM1 = 0, K_APP1, K_APP2, K_BOP1, K_BOP2, K_IF1, K_IF2 };

struct K;

struct K_App1 {
  ast::Exp<raw_ptr> *x;
  K *k;
};

struct K_App2 {
  anf::Value f;
  K *k;
};

struct K_Bop1 {
  ast::Exp<raw_ptr> *y;
  ast::Bop bop;
  K *k;
};

struct K_Bop2 {
  anf::Value y;
  ast::Bop bop;
  K *k;
};

struct K {
  K_Tag tag;
  union {
    K_App1 app1;
    K_App2 app2;
  };
};

} // namespace lambcalc
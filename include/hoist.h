#ifndef HOIST_H
#define HOIST_H

#include "anf.h"

namespace lambcalc {
namespace anf {

struct Join {
  Var name;
  std::optional<Var> slot;
  std::unique_ptr<anf::Exp> body;
};

struct Function {
  Var name;
  std::vector<Var> params;
  Join entryBlock;
  std::vector<Join> blocks;
};

std::vector<Function> hoist(std::unique_ptr<anf::Exp> &&exp);

} // namespace anf
} // namespace lambcalc

#endif
#ifndef CONVERT_H
#define CONVERT_H

#include "anf.h"
#include <set>

namespace lambcalc {
namespace convert {

std::set<anf::Var> freeVars(anf::Exp &exp);

} // namespace convert
} // namespace lambcalc

#endif
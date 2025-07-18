#ifndef CONVERT_H
#define CONVERT_H

#include "anf.h"
#include <set>

namespace lambcalc {
namespace convert {

std::set<anf::Var> freeVars(anf::Exp &exp);
std::unique_ptr<anf::Exp> closureConvert(std::unique_ptr<anf::Exp> &&start);

} // namespace convert
} // namespace lambcalc

#endif
#ifndef ANF_COMPUTED_GOTO_H
#define ANF_COMPUTED_GOTO_H

#include "anf.h"

namespace lambcalc {
namespace anf {

template <template <class> class Ptr>
std::unique_ptr<Exp> convertComputedGoto(SymbolTable &table, ast::Exp<Ptr> &root);

}
} // namespace lambcalc

#endif // ANF_COMPUTED_GOTO_H
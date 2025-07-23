#ifndef LOWER_H
#define LOWER_H

#include "hoist.h"
#include "llvm/IR/Module.h"

namespace lambcalc {
namespace anf {

std::unique_ptr<llvm::Module> lower(std::vector<Function> &&fns);

}
} // namespace lambcalc

#endif
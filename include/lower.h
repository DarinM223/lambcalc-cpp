#ifndef LOWER_H
#define LOWER_H

#include "hoist.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"

namespace lambcalc {
namespace lower {

extern std::unique_ptr<llvm::LLVMContext> ctx;
extern std::unique_ptr<llvm::LoopAnalysisManager> lam;
extern std::unique_ptr<llvm::FunctionPassManager> fpm;
extern std::unique_ptr<llvm::FunctionAnalysisManager> fam;
extern std::unique_ptr<llvm::CGSCCAnalysisManager> cgam;
extern std::unique_ptr<llvm::ModuleAnalysisManager> mam;
extern std::unique_ptr<llvm::PassInstrumentationCallbacks> pic;
extern std::unique_ptr<llvm::StandardInstrumentations> si;

std::unique_ptr<llvm::Module> lower(std::vector<anf::Function> &&fns);
std::unique_ptr<llvm::Module> lower(std::vector<anf::Function> &&fns,
                                    const llvm::DataLayout &layout);

} // namespace lower
} // namespace lambcalc

#endif
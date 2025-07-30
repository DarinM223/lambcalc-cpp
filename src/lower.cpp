#include "lower.h"
#include "utils.h"
#include "visitor.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"

namespace lambcalc {
namespace lower {

using namespace llvm;
using namespace anf;

template <typename T>
llvm::FunctionType *getFunctionType(IRBuilder<> &builder,
                                    const std::vector<T> &params) {
  std::vector<Type *> tys;
  for (size_t i = 0; i < params.size(); ++i) {
    if (i == 0) {
      tys.push_back(builder.getPtrTy());
    } else {
      tys.push_back(builder.getInt64Ty());
    }
  }
  return llvm::FunctionType::get(builder.getInt64Ty(), tys, false);
}

using LLVMLowerPipeline =
    MatchIfJump<WorklistVisitor<ExpValueVisitor<DefaultExpVisitor>,
                                WorklistTask, std::stack>>;
class LLVMLowerVisitor : public LLVMLowerPipeline {
  LLVMContext &ctx_;
  Module &module_;
  IRBuilder<> &builder_;
  llvm::Value *value_;
  llvm::StringMap<llvm::AllocaInst *> &spillSlots_;
  llvm::StringMap<llvm::Value *> &namedValues_;
  llvm::StringMap<llvm::BasicBlock *> &namedBlocks_;

public:
  LLVMLowerVisitor(LLVMContext &ctx, Module &module, IRBuilder<> &builder,
                   llvm::StringMap<llvm::AllocaInst *> &spillSlots,
                   llvm::StringMap<llvm::Value *> &namedValues,
                   llvm::StringMap<llvm::BasicBlock *> &namedBlocks)
      : ctx_(ctx), module_(module), builder_(builder), value_(nullptr),
        spillSlots_(spillSlots), namedValues_(namedValues),
        namedBlocks_(namedBlocks) {}
  using LLVMLowerPipeline::operator();
  void visitValue(IntValue &value) { value_ = builder_.getInt64(value.value); }
  void visitValue(VarValue &value) { value_ = namedValues_.lookup(value.var); }
  void visitValue(GlobValue &value) {
    llvm::Function *function;
    llvm::GlobalVariable *global;
    if ((function = module_.getFunction(value.glob))) {
      value_ = builder_.CreatePtrToInt(function, builder_.getInt64Ty());
    } else if ((global = module_.getGlobalVariable(value.glob))) {
      value_ = builder_.CreatePtrToInt(global, builder_.getInt64Ty());
    } else {
      value_ = module_.getOrInsertGlobal(value.glob, builder_.getInt64Ty());
    }
  }

  void operator()(HaltExp &exp) {
    std::visit(*this, exp.value);
    builder_.CreateRet(value_);
  }
  void operator()(JumpExp &exp) {
    auto block = namedBlocks_.lookup(exp.joinName);
    if (exp.slotValue) {
      auto slot = spillSlots_[exp.joinName];
      std::visit(*this, *exp.slotValue);
      builder_.CreateStore(value_, slot);
    }
    builder_.CreateBr(block);
  }
  void operator()(AppExp &exp) {
    std::vector<llvm::Value *> params;
    for (auto &val : exp.paramValues) {
      std::visit(*this, val);
      params.push_back(value_);
    }
    if (namedValues_.contains(exp.funName)) {
      auto fty = getFunctionType(builder_, exp.paramValues);
      auto fPtrTy = PointerType::get(fty, 0);
      auto fnPtrInt = namedValues_[exp.funName];
      auto fn = builder_.CreateIntToPtr(fnPtrInt, fPtrTy);
      namedValues_[exp.name] = builder_.CreateCall(fty, fn, params, exp.name);
    } else {
      auto fn = module_.getFunction(exp.funName);
      namedValues_[exp.name] = builder_.CreateCall(fn, params, exp.name);
    }
    return LLVMLowerPipeline::operator()(exp);
  }
  void operator()(BopExp &exp) {
    std::visit(*this, exp.param1);
    auto param1 = value_;
    std::visit(*this, exp.param2);
    auto param2 = value_;
    llvm::Instruction::BinaryOps bop;
    switch (exp.bop) {
    case ast::Bop::Plus:
      bop = llvm::Instruction::BinaryOps::Add;
      break;
    case ast::Bop::Minus:
      bop = llvm::Instruction::BinaryOps::Sub;
      break;
    case ast::Bop::Times:
      bop = llvm::Instruction::BinaryOps::Mul;
      break;
    }
    namedValues_[exp.name] =
        builder_.CreateBinOp(bop, param1, param2, exp.name);
    return LLVMLowerPipeline::operator()(exp);
  }
  void operator()(TupleExp &exp) {
    auto mallocTy =
        FunctionType::get(builder_.getPtrTy(), builder_.getInt64Ty(), false);
    auto malloc = module_.getOrInsertFunction("malloc", mallocTy);
    auto ptr = builder_.CreateCall(
        malloc, {builder_.getInt64(exp.values.size() * 8)}, exp.name);
    namedValues_[exp.name] =
        builder_.CreatePtrToInt(ptr, builder_.getInt64Ty());
    for (size_t i = 0; i < exp.values.size(); ++i) {
      auto value = exp.values[i];
      // getElementPtr just returns the address based off of indexing the
      // pointer. That address can be used for a store instruction.
      auto gep = builder_.CreateGEP(builder_.getInt64Ty(), ptr,
                                    {builder_.getInt64(i)});
      std::visit(*this, value);
      builder_.CreateStore(value_, gep);
    }
    return LLVMLowerPipeline::operator()(exp);
  }
  void operator()(ProjExp &exp) {
    auto tupleInt = namedValues_[exp.tuple];
    auto tuplePtr = builder_.CreateIntToPtr(tupleInt, builder_.getPtrTy());
    auto gep = builder_.CreateGEP(builder_.getInt64Ty(), tuplePtr,
                                  {builder_.getInt64(exp.index)});
    namedValues_[exp.name] =
        builder_.CreateLoad(builder_.getInt64Ty(), gep, exp.name);
    return LLVMLowerPipeline::operator()(exp);
  }

  void visitIfJump(IfExp &exp, JumpExp &thenJump, JumpExp &elseJump) override {
    std::visit(*this, exp.cond);
    auto cond = builder_.CreateICmpNE(value_, builder_.getInt64(0));
    if (thenJump.slotValue) {
      builder_.CreateStore(value_, spillSlots_[thenJump.joinName]);
    }
    if (elseJump.slotValue) {
      builder_.CreateStore(value_, spillSlots_[elseJump.joinName]);
    }
    builder_.CreateCondBr(cond, namedBlocks_[thenJump.joinName],
                          namedBlocks_[elseJump.joinName]);
  }
};

static void lowerBlock(LLVMLowerVisitor &visitor, Join &block) {
  auto &worklist = visitor.getWorklist();
  worklist.emplace(&block.body, *block.body);
  while (!worklist.empty()) {
    auto task = std::move(worklist.top());
    worklist.pop();
    std::visit(overloaded{[&](NodeTask &task) {
                            auto [parent, exp] = task;
                            std::visit(visitor, static_cast<Exp &>(exp));
                          },
                          [](FnTask &fn) { fn(); }},
               task);
  }
}

// Context needs a global scope or else if it gets freed
// it takes the module with it.
std::unique_ptr<LLVMContext> ctx;
std::unique_ptr<LoopAnalysisManager> lam;
std::unique_ptr<FunctionPassManager> fpm;
std::unique_ptr<FunctionAnalysisManager> fam;
std::unique_ptr<CGSCCAnalysisManager> cgam;
std::unique_ptr<ModuleAnalysisManager> mam;
std::unique_ptr<PassInstrumentationCallbacks> pic;
std::unique_ptr<StandardInstrumentations> si;

std::unique_ptr<Module> initializeModuleAndManagers() {
  ctx = std::make_unique<LLVMContext>();
  fpm = std::make_unique<FunctionPassManager>();
  lam = std::make_unique<LoopAnalysisManager>();
  fam = std::make_unique<FunctionAnalysisManager>();
  cgam = std::make_unique<CGSCCAnalysisManager>();
  mam = std::make_unique<ModuleAnalysisManager>();
  pic = std::make_unique<PassInstrumentationCallbacks>();
  si = std::make_unique<StandardInstrumentations>(*ctx, true);
  si->registerCallbacks(*pic, mam.get());

  fpm->addPass(PromotePass());
  fpm->addPass(InstCombinePass());
  fpm->addPass(ReassociatePass());
  fpm->addPass(GVNPass());
  fpm->addPass(SimplifyCFGPass());

  PassBuilder PB;
  PB.registerModuleAnalyses(*mam);
  PB.registerFunctionAnalyses(*fam);
  PB.crossRegisterProxies(*lam, *fam, *cgam, *mam);

  return std::make_unique<Module>("lambcalc program", *ctx);
}

std::unique_ptr<Module> initializeModuleAndManagers(const DataLayout &layout) {
  auto mod = initializeModuleAndManagers();
  mod->setDataLayout(layout);
  return mod;
}

void lowerModule(std::vector<Function> &&fns, Module &module) {
  auto builder = std::make_unique<IRBuilder<>>(*ctx);
  for (auto &fn : fns) {
    // getPtrTy() gets an opaque pointer, which is preferred for modern LLVM
    // than a typed pointer.
    auto ty = getFunctionType(*builder, fn.params);
    llvm::Function::Create(ty, llvm::GlobalValue::ExternalLinkage, fn.name,
                           module);
  }
  for (auto &fn : fns) {
    StringMap<llvm::AllocaInst *> spillSlots;
    StringMap<llvm::Value *> namedValues;
    StringMap<llvm::BasicBlock *> namedBlocks;
    LLVMLowerVisitor visitor(*ctx, module, *builder, spillSlots, namedValues,
                             namedBlocks);

    auto loweredFn = module.getFunction(fn.name);
    auto loweredEntryBlock =
        BasicBlock::Create(*ctx, fn.entryBlock.name, loweredFn);
    namedBlocks[fn.entryBlock.name] = loweredEntryBlock;
    builder->SetInsertPoint(loweredEntryBlock);
    for (auto &block : fn.blocks) {
      auto loweredBlock = BasicBlock::Create(*ctx, block.name, loweredFn);
      namedBlocks[block.name] = loweredBlock;
      // preprocess spill slots
      if (block.slot) {
        spillSlots[block.name] = builder->CreateAlloca(builder->getInt64Ty());
      }
    }
    size_t i = 0;
    for (auto &arg : loweredFn->args()) {
      const std::string &param = fn.params[i++];
      arg.setName(param);
      namedValues[param] = &arg;
    }

    lowerBlock(visitor, fn.entryBlock);
    for (auto &block : fn.blocks) {
      auto loweredBlock = namedBlocks[block.name];
      builder->SetInsertPoint(loweredBlock);
      if (block.slot) {
        auto slot = spillSlots[block.name];
        namedValues[*block.slot] =
            builder->CreateLoad(builder->getInt64Ty(), slot, *block.slot);
      }
      lowerBlock(visitor, block);
    }
    llvm::verifyFunction(*loweredFn);
  }
}

std::unique_ptr<Module> lower(std::vector<Function> &&fns) {
  auto module = initializeModuleAndManagers();
  lowerModule(std::move(fns), *module);
  return module;
}

std::unique_ptr<Module> lower(std::vector<Function> &&fns,
                              const DataLayout &layout) {
  auto module = initializeModuleAndManagers(layout);
  lowerModule(std::move(fns), *module);
  return module;
}

} // namespace lower
} // namespace lambcalc
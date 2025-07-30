#include "KaleidoscopeJIT.h"
#include "anf.h"
#include "ast.h"
#include "convert.h"
#include "hoist.h"
#include "lower.h"
#include "parser.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include <iostream>

using namespace lambcalc;

static llvm::ExitOnError ExitOnErr;

const std::unordered_map<ast::Bop, std::optional<std::pair<int, int>>>
    defaultInfixBp{{ast::Bop::Plus, {{1, 2}}},
                   {ast::Bop::Minus, {{1, 2}}},
                   {ast::Bop::Times, {{3, 4}}}};

int main() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  std::unique_ptr<llvm::orc::KaleidoscopeJIT> jit =
      ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
  Lexer lexer(std::cin);
  Parser parser(lexer, defaultInfixBp);
  while (true) {
    std::cout << "> ";
    std::unique_ptr<ast::Exp> exp;
    try {
      exp = parser.parseExpression();
    } catch (ParserException &e) {
      continue;
    }
    auto anf = anf::convert(*exp);
    auto convert = convert::closureConvert(std::move(anf));
    auto hoisted = anf::hoist(std::move(convert));
    auto mod = lower::lower(std::move(hoisted), jit->getDataLayout());
    mod->dump();

    auto rt = jit->getMainJITDylib().createResourceTracker();
    auto tsm =
        llvm::orc::ThreadSafeModule(std::move(mod), std::move(lower::ctx));
    ExitOnErr(jit->addModule(std::move(tsm), rt));

    auto exprSymbol = ExitOnErr(jit->lookup("main"));
    int (*FP)() = exprSymbol.getAddress().toPtr<int (*)()>();
    std::cout << "Evaluated to: " << FP() << std::endl;
    ExitOnErr(rt->remove());
  }
  return 0;
}
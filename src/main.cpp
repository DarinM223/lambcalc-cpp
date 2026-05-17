#include "KaleidoscopeJIT.h"
#include "anf.h"
#include "arena.h"
#include "ast.h"
#include "convert.h"
#include "hoist.h"
#include "lower.h"
#include "parser.h"
#include "rename.h"
#include "symbol.h"
#include "utils.h"
#include "llvm/Support/TargetSelect.h"
#include <iostream>
#include <memory>

using namespace lambcalc;

static llvm::ExitOnError ExitOnErr;

constexpr bool LAMBCALC_DEBUG = false;

const std::unordered_map<ast::Bop, std::optional<std::pair<int, int>>>
    defaultInfixBp{{ast::Bop::Plus, {{1, 2}}},
                   {ast::Bop::Minus, {{1, 2}}},
                   {ast::Bop::Times, {{3, 4}}}};

int main() {
  SymbolTable table;
  arena::LinkedAllocator allocator(1 << 28);
  arena::Typed<ast::Exp<raw_ptr>, decltype(allocator)> expAlloc(allocator);

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  std::unique_ptr<llvm::orc::KaleidoscopeJIT> jit =
      ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
  Lexer lexer(table, std::cin);
  Parser<raw_ptr, decltype(expAlloc)> parser(expAlloc, lexer, defaultInfixBp);
  while (true) {
    allocator.reset();
    // If a peek token is already buffered, consume it.
    if (parser.getPeekToken() && *parser.getPeekToken() == Token::Semicolon) {
      parser.nextToken();
      continue;
    }
    std::cout << "> ";
    ast::Exp<raw_ptr> *exp;
    try {
      // If it reads a semicolon token at the start, go back to
      // beginning to consume it.
      if (parser.peekToken() == Token::Semicolon) {
        continue;
      }
      exp = parser.parseExpression();
    } catch (ParserException &e) {
      std::cerr << e.what() << "\n";
      if (e.abort()) {
        break;
      } else {
        continue;
      }
    }
    if constexpr (LAMBCALC_DEBUG) {
      std::cout << exp->dump(table) << "\n";
    }
    try {
      ast::rename(table, *exp);
    } catch (ast::NotInScopeException &e) {
      std::cerr << e.what() << "\n";
      continue;
    }
    if constexpr (LAMBCALC_DEBUG) {
      std::cout << "After renaming: " << exp->dump(table) << "\n";
    }
    auto anf = anf::convertDefunc(table, *exp);
    auto convert = convert::closureConvert(table, std::move(anf));
    if constexpr (LAMBCALC_DEBUG) {
      std::cout << convert->dump(table) << "\n";
    }

    auto hoisted = anf::hoist(table, std::move(convert));
    if constexpr (LAMBCALC_DEBUG) {
      for (const auto &fn : hoisted) {
        std::cout << fn.name << "( ";
        for (const auto &param : fn.params) {
          std::cout << param << " ";
        }
        std::cout << "):" << std::endl;
        std::cout << fn.entryBlock.name << " < ";
        if (fn.entryBlock.slot) {
          std::cout << *fn.entryBlock.slot;
        }
        std::cout << " >: " << "\n" << fn.entryBlock.body->dump(table) << "\n";
        for (const auto &block : fn.blocks) {
          std::cout << block.name << " < ";
          if (block.slot) {
            std::cout << *block.slot;
          }
          std::cout << " >: " << "\n" << block.body->dump(table) << "\n";
        }

        std::cout << std::endl;
      }
    }
    auto mod = lower::lower(table, std::move(hoisted), jit->getDataLayout());
    if constexpr (LAMBCALC_DEBUG) {
      mod->dump();
    }

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
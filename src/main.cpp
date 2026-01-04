#include "KaleidoscopeJIT.h"
#include "anf.h"
#include "arena.h"
#include "ast.h"
#include "convert.h"
#include "hoist.h"
#include "lower.h"
#include "parser.h"
#include "rename.h"
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
  alignas(alignof(ast::Exp<raw_ptr>)) static char buf[1 << 28];
  char *ptr = std::launder(buf);
  arena::Allocator allocator(ptr, ptr + sizeof(buf) / sizeof(*buf));
  arena::TypedAllocator<ast::Exp<raw_ptr>> typedAllocator(allocator);

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  std::unique_ptr<llvm::orc::KaleidoscopeJIT> jit =
      ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
  Lexer lexer(std::cin);
  Parser<raw_ptr, arena::TypedAllocator<ast::Exp<raw_ptr>>> parser(
      typedAllocator, lexer, defaultInfixBp);
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
      std::cerr << e.what() << std::endl;
      if (e.abort()) {
        break;
      } else {
        continue;
      }
    }
    if constexpr (LAMBCALC_DEBUG) {
      std::cout << *exp << std::endl;
    }
    try {
      ast::rename(*exp);
    } catch (ast::NotInScopeException &e) {
      std::cerr << e.what() << std::endl;
      continue;
    }
    if constexpr (LAMBCALC_DEBUG) {
      std::cout << "After renaming: " << *exp << std::endl;
    }
    auto anf = anf::convertDefunc(*exp);
    auto convert = convert::closureConvert(std::move(anf));
    if constexpr (LAMBCALC_DEBUG) {
      std::cout << *convert << std::endl;
    }

    auto hoisted = anf::hoist(std::move(convert));
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
        std::cout << " >: " << std::endl << *fn.entryBlock.body << std::endl;
        for (const auto &block : fn.blocks) {
          std::cout << block.name << " < ";
          if (block.slot) {
            std::cout << *block.slot;
          }
          std::cout << " >: " << std::endl << *block.body << std::endl;
        }

        std::cout << std::endl;
      }
    }
    auto mod = lower::lower(std::move(hoisted), jit->getDataLayout());
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
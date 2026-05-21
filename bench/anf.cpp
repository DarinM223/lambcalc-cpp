#include "anf.h"
#include "anf_computed_goto.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include <benchmark/benchmark.h>
#include <sstream>

namespace lambcalc {

using namespace ast;

const std::unordered_map<ast::Bop, std::optional<std::pair<int, int>>>
    defaultInfixBp{{ast::Bop::Plus, {{1, 2}}},
                   {ast::Bop::Minus, {{1, 2}}},
                   {ast::Bop::Times, {{3, 4}}}};

auto getTestExp(SymbolTable &table) {
  std::string start =
      "(fn g => (fn x => g (fn v => x x v)) (fn x => g (fn v => x x v))) (fn "
      "f "
      "=> fn x => if x then (if x - 1 then x * f (x - 1) else 1) else 1) 5";
  std::string build = start;
  for (size_t i = 0; i < 100; i++) {
    build += "+" + start;
  }
  std::istringstream is(build);
  std::allocator<ast::Exp<>> allocator;
  Lexer lexer(table, is);
  Parser parser(allocator, lexer, defaultInfixBp);

  return parser.parseExpression();
}

static void BM_Anf(benchmark::State &state) {
  SymbolTable table;
  auto exp = getTestExp(table);
  for (auto _ : state) {
    anf::resetCounter();
    auto anf = anf::convert(table, *exp);
  }
}
BENCHMARK(BM_Anf);

static void BM_AnfDefunc(benchmark::State &state) {
  SymbolTable table;
  auto exp = getTestExp(table);
  for (auto _ : state) {
    anf::resetCounter();
    auto anf = anf::convertDefunc(table, *exp);
  }
}
BENCHMARK(BM_AnfDefunc);

static void BM_AnfComputedGoto(benchmark::State &state) {
  SymbolTable table;
  auto exp = getTestExp(table);
  for (auto _ : state) {
    anf::resetCounter();
    auto anf = anf::convertComputedGoto(table, *exp);
  }
}
BENCHMARK(BM_AnfComputedGoto);

} // namespace lambcalc
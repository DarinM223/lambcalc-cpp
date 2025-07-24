#include "lower.h"
#include "llvm/Support/raw_os_ostream.h"
#include <gtest/gtest.h>

namespace lambcalc {

using namespace anf;
using namespace llvm;

TEST(Lower, Simple) {
  auto exp = make(BopExp{"c", ast::Bop::Plus, IntValue{1}, IntValue{2},
                         make(HaltExp{VarValue{"c"}})});
  auto hoisted = anf::hoist(std::move(exp));
  auto lowered = anf::lower(std::move(hoisted));
  std::ostringstream out;
  llvm::raw_os_ostream rout(out);
  lowered->print(rout, nullptr);
  std::string expected = "; ModuleID = 'lambcalc program'\n"
                         "source_filename = \"lambcalc program\"\n\n"
                         "define i64 @main() {\n"
                         "entry0:\n"
                         "  ret i64 3\n"
                         "}\n";
  EXPECT_EQ(out.str(), expected);
}

TEST(Lower, Functions) {
  auto exp = make(FunExp{
      "f1",
      {"a"},
      make(JoinExp{
          "j1",
          {},
          make(FunExp{
              "f2",
              {"b"},
              make(JoinExp{"j2",
                           {"c"},
                           make(HaltExp{VarValue{"c"}}),
                           make(JumpExp{"j2", {IntValue{0}}})}),
              make(AppExp{
                  "y", "f2", {IntValue{1}}, make(HaltExp{VarValue{"y"}})})}),
          make(JoinExp{"j3", {}, make(JumpExp{"j1"}), make(JumpExp{"j3"})})}),
      make(AppExp{"x", "f1", {IntValue{0}}, make(HaltExp{VarValue{"x"}})})});
  auto hoisted = anf::hoist(std::move(exp));
  auto lowered = anf::lower(std::move(hoisted));
  std::ostringstream out;
  llvm::raw_os_ostream rout(out);
  lowered->print(rout, nullptr);
  std::string expected =
      "; ModuleID = 'lambcalc program'\n"
      "source_filename = \"lambcalc program\"\n"
      "\n"
      "define i64 @f2(ptr %b) {\n"
      "entry0:\n"
      "  %0 = alloca i64, align 8\n"
      "  store i64 0, ptr %0, align 4\n"
      "  br label %j2\n"
      "\n"
      "j2:                                               ; preds = %entry0\n"
      "  %c = load i64, ptr %0, align 4\n"
      "  ret i64 %c\n"
      "}\n"
      "\n"
      "define i64 @f1(ptr %a) {\n"
      "entry1:\n"
      "  br label %j3\n"
      "\n"
      "j1:                                               ; preds = %j3\n"
      "  %y = call i64 @f2(i64 1)\n"
      "  ret i64 %y\n"
      "\n"
      "j3:                                               ; preds = %entry1\n"
      "  br label %j1\n"
      "}\n"
      "\n"
      "define i64 @main() {\n"
      "entry2:\n"
      "  %x = call i64 @f1(i64 0)\n"
      "  ret i64 %x\n"
      "}\n";
  EXPECT_EQ(out.str(), expected);
}

TEST(Lower, IfElse) {}

} // namespace lambcalc
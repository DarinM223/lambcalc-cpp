#include "lower.h"
#include "convert.h"
#include "llvm/Support/raw_os_ostream.h"
#include <gtest/gtest.h>

namespace lambcalc {

using namespace anf;
using namespace llvm;

TEST(Lower, Simple) {
  auto exp = make(BopExp{"c", ast::Bop::Plus, IntValue{1}, IntValue{2},
                         make(HaltExp{VarValue{"c"}})});
  auto hoisted = anf::hoist(std::move(exp));
  auto lowered = lower::lower(std::move(hoisted));
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
  auto lowered = lower::lower(std::move(hoisted));
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

TEST(Lower, IfElse) {
  auto exp = make(FunExp{
      "f",
      {"closure", "x"},
      make(BopExp{
          "a", ast::Bop::Plus, IntValue{1}, IntValue{2},
          make(IfExp{
              VarValue{"a"},
              make(IfExp{
                  VarValue{"x"},
                  make(BopExp{"c", ast::Bop::Plus, VarValue{"x"}, VarValue{"a"},
                              make(AppExp{"d",
                                          "f",
                                          {VarValue{"c"}},
                                          make(HaltExp{VarValue{"d"}})})}),
                  make(HaltExp{IntValue{5}})}),
              make(HaltExp{VarValue{"a"}})})}),
      make(AppExp{"b",
                  "f",
                  {IntValue{0}, IntValue{1}}, // Pass 0 for closure for now.
                  make(HaltExp{VarValue{"b"}})})});
  auto hoisted = anf::hoist(std::move(exp));
  auto lowered = lower::lower(std::move(hoisted));
  std::ostringstream out;
  llvm::raw_os_ostream rout(out);
  lowered->print(rout, nullptr);
  std::string expected = "; ModuleID = 'lambcalc program'\n"
                         "source_filename = \"lambcalc program\"\n"
                         "\n"
                         "define i64 @f(ptr %closure, i64 %x) {\n"
                         "entry4:\n"
                         "  br i1 true, label %then2, label %else3\n"
                         "\n"
                         "then0:                                            ; "
                         "preds = %then2\n"
                         "  %c = add i64 %x, 3\n"
                         "  %d = call i64 @f(i64 %c)\n"
                         "  ret i64 %d\n"
                         "\n"
                         "else1:                                            ; "
                         "preds = %then2\n"
                         "  ret i64 5\n"
                         "\n"
                         "then2:                                            ; "
                         "preds = %entry4\n"
                         "  %0 = icmp ne i64 %x, 0\n"
                         "  br i1 %0, label %then0, label %else1\n"
                         "\n"
                         "else3:                                            ; "
                         "preds = %entry4\n"
                         "  ret i64 3\n"
                         "}\n"
                         "\n"
                         "define i64 @main() {\n"
                         "entry5:\n"
                         "  %b = call i64 @f(i64 0, i64 1)\n"
                         "  ret i64 %b\n"
                         "}\n";
  EXPECT_EQ(out.str(), expected);
}

TEST(Lower, AppClosure) {
  // (fn g => (fn x => g x) (fn y => g y))
  // let f1 = fn g =>
  //   let f2 = fn x =>
  //     let t1 = g x in
  //     t1
  //   in
  //   let f3 = fn y =>
  //     let t2 = g y in
  //     t2
  //   in
  //   let t3 = f2 f3 in
  //   t3
  // in
  // f1
  auto exp = make(FunExp{
      "f1",
      {"g"},
      make(FunExp{
          "f2",
          {"x"},
          make(AppExp{
              "t1", "g", {VarValue{"x"}}, make(HaltExp{VarValue{"t1"}})}),
          make(FunExp{
              "f3",
              {"y"},
              make(AppExp{
                  "t2", "g", {VarValue{"y"}}, make(HaltExp{VarValue{"t2"}})}),
              make(AppExp{"t3",
                          "f2",
                          {VarValue{"f3"}},
                          make(HaltExp{VarValue{"t3"}})})})}),
      make(HaltExp{VarValue{"f1"}})});
  auto converted = convert::closureConvert(std::move(exp));
  std::string expectedANF =
      "FunExp { f1, [closure0, g], FunExp { f2, [closure1, x], ProjExp { g, "
      "closure1, 1, ProjExp { proj5, g, 0, AppExp { t1, proj5, [g, x], HaltExp "
      "{ t1 } } } }, TupleExp { f2, [f2, g], FunExp { f3, [closure2, y], "
      "ProjExp { g, closure2, 2, ProjExp { f2, closure2, 1, ProjExp { proj4, "
      "g, 0, AppExp { t2, proj4, [g, y], HaltExp { t2 } } } } }, TupleExp { "
      "f3, [f3, f2, g], ProjExp { proj3, f2, 0, AppExp { t3, proj3, [f2, f3], "
      "HaltExp { t3 } } } } } } }, TupleExp { f1, [f1], HaltExp { f1 } } }";
  EXPECT_EQ(converted->dump(), expectedANF);

  auto hoisted = anf::hoist(std::move(converted));
  auto lowered = lower::lower(std::move(hoisted));
  std::ostringstream out;
  llvm::raw_os_ostream rout(out);
  lowered->print(rout, nullptr);
  std::string expectedLLVM =
      "; ModuleID = 'lambcalc program'\n"
      "source_filename = \"lambcalc program\"\n"
      "\n"
      "define i64 @f2(ptr %closure1, i64 %x) {\n"
      "entry0:\n"
      "  %0 = getelementptr i64, ptr %closure1, i64 1\n"
      "  %g = load i64, ptr %0, align 4\n"
      "  %1 = inttoptr i64 %g to ptr\n"
      "  %2 = getelementptr i64, ptr %1, i64 0\n"
      "  %proj5 = load i64, ptr %2, align 4\n"
      "  %3 = inttoptr i64 %proj5 to ptr\n"
      "  %t1 = call i64 %3(i64 %g, i64 %x)\n"
      "  ret i64 %t1\n"
      "}\n"
      "\n"
      "define i64 @f3(ptr %closure2, i64 %y) {\n"
      "entry1:\n"
      "  %0 = getelementptr i64, ptr %closure2, i64 2\n"
      "  %g = load i64, ptr %0, align 4\n"
      "  %1 = getelementptr i64, ptr %closure2, i64 1\n"
      "  %f2 = load i64, ptr %1, align 4\n"
      "  %2 = inttoptr i64 %g to ptr\n"
      "  %3 = getelementptr i64, ptr %2, i64 0\n"
      "  %proj4 = load i64, ptr %3, align 4\n"
      "  %4 = inttoptr i64 %proj4 to ptr\n"
      "  %t2 = call i64 %4(i64 %g, i64 %y)\n"
      "  ret i64 %t2\n"
      "}\n"
      "\n"
      "define i64 @f1(ptr %closure0, i64 %g) {\n"
      "entry2:\n"
      "  %f2 = call ptr @malloc(i64 16)\n"
      "  %0 = ptrtoint ptr %f2 to i64\n"
      "  %1 = getelementptr i64, ptr %f2, i64 0\n"
      "  store i64 ptrtoint (ptr @f2 to i64), ptr %1, align 4\n"
      "  %2 = getelementptr i64, ptr %f2, i64 1\n"
      "  store i64 %g, ptr %2, align 4\n"
      "  %f3 = call ptr @malloc(i64 24)\n"
      "  %3 = ptrtoint ptr %f3 to i64\n"
      "  %4 = getelementptr i64, ptr %f3, i64 0\n"
      "  store i64 ptrtoint (ptr @f3 to i64), ptr %4, align 4\n"
      "  %5 = getelementptr i64, ptr %f3, i64 1\n"
      "  store i64 %0, ptr %5, align 4\n"
      "  %6 = getelementptr i64, ptr %f3, i64 2\n"
      "  store i64 %g, ptr %6, align 4\n"
      "  %7 = inttoptr i64 %0 to ptr\n"
      "  %8 = getelementptr i64, ptr %7, i64 0\n"
      "  %proj3 = load i64, ptr %8, align 4\n"
      "  %9 = inttoptr i64 %proj3 to ptr\n"
      "  %t3 = call i64 %9(i64 %0, i64 %3)\n"
      "  ret i64 %t3\n"
      "}\n"
      "\n"
      "define i64 @main() {\n"
      "entry3:\n"
      "  %f1 = call ptr @malloc(i64 8)\n"
      "  %0 = ptrtoint ptr %f1 to i64\n"
      "  %1 = getelementptr i64, ptr %f1, i64 0\n"
      "  store i64 ptrtoint (ptr @f1 to i64), ptr %1, align 4\n"
      "  ret i64 %0\n"
      "}\n"
      "\n"
      "declare ptr @malloc(i64)\n";
  EXPECT_EQ(out.str(), expectedLLVM);
}

} // namespace lambcalc
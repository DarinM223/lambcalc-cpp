#include "visitor.h"
#include "utils.h"
#include <utility>
#include <variant>

namespace lambcalc {

std::string binOpString(ast::Bop bop) {
  switch (bop) {
  case ast::Bop::Plus:
    return "+";
  case ast::Bop::Minus:
    return "-";
  case ast::Bop::Times:
    return "*";
  }
  std::unreachable();
}

void print(const SymbolTable &table, std::ostream &os, Symbol sym) {
  os << table.lookup(sym);
}

namespace ast {

template <template <class> class Ptr>
void print(const SymbolTable &table, std::ostream &os, const Exp<Ptr> &exp) {
  std::visit(PrintExpVisitor<Ptr>(table, os), exp);
}

template <template <class> class Ptr>
void PrintExpVisitor<Ptr>::operator()(const IntExp &exp) {
  out_ << exp.value;
}
template <template <class> class Ptr>
void PrintExpVisitor<Ptr>::operator()(const VarExp &exp) {
  out_ << table_.lookup(exp.name);
}
template <template <class> class Ptr>
void PrintExpVisitor<Ptr>::operator()(const LamExp<Ptr> &exp) {
  out_ << "(fn " << table_.lookup(exp.param) << " => ";
  print(table_, out_, *exp.body);
  out_ << ")";
}
template <template <class> class Ptr>
void PrintExpVisitor<Ptr>::operator()(const AppExp<Ptr> &exp) {
  out_ << "(";
  print(table_, out_, *exp.fn);
  out_ << " ";
  print(table_, out_, *exp.arg);
  out_ << ")";
}
template <template <class> class Ptr>
void PrintExpVisitor<Ptr>::operator()(const BopExp<Ptr> &exp) {
  out_ << "(";
  print(table_, out_, *exp.arg1);
  out_ << " " << binOpString(exp.bop) << " ";
  print(table_, out_, *exp.arg2);
  out_ << ")";
}
template <template <class> class Ptr>
void PrintExpVisitor<Ptr>::operator()(const IfExp<Ptr> &exp) {
  out_ << "(if ";
  print(table_, out_, *exp.cond);
  out_ << " then ";
  print(table_, out_, *exp.then);
  out_ << " else ";
  print(table_, out_, *exp.els);
  out_ << ")";
}

template class PrintExpVisitor<std::unique_ptr>;
template class PrintExpVisitor<raw_ptr>;

} // namespace ast

namespace anf {

template <typename T>
void print_vector(const SymbolTable &table, std::ostream &os,
                  const std::vector<T> &vec) {
  os << "[";
  for (auto it = vec.begin(); it != vec.end(); ++it) {
    print(table, os, *it);
    if (it + 1 != vec.end()) {
      os << ", ";
    }
  }
  os << "]";
}

template <typename T>
void print_optional(const SymbolTable &table, std::ostream &os,
                    std::optional<T> opt) {
  if (opt) {
    os << "<";
    print(table, os, *opt);
    os << ">";
  } else {
    os << "<>";
  }
}

void PrintValueVisitor::operator()(const IntValue &value) {
  out_ << value.value;
}
void PrintValueVisitor::operator()(const VarValue &value) {
  out_ << table_.lookup(value.var);
}
void PrintValueVisitor::operator()(const GlobValue &value) {
  out_ << table_.lookup(value.glob);
}

void print(const SymbolTable &table, std::ostream &os, const Value &value) {
  std::visit(PrintValueVisitor(table, os), value);
}

void print(const SymbolTable &table, std::ostream &os, const Exp &exp) {
  std::visit(PrintExpVisitor(table, os), exp);
}

void PrintExpVisitor::operator()(const HaltExp &exp) {
  out_ << "HaltExp { ";
  print(table_, out_, exp.value);
  out_ << " }";
}

void PrintExpVisitor::operator()(const FunExp &exp) {
  out_ << "FunExp { " << table_.lookup(exp.name) << ", ";
  print_vector(table_, out_, exp.params);
  out_ << ", ";
  print(table_, out_, *exp.body);
  out_ << ", ";
  print(table_, out_, *exp.rest);
  out_ << " }";
}

void PrintExpVisitor::operator()(const JoinExp &exp) {
  out_ << "JoinExp { " << table_.lookup(exp.name) << ", ";
  print_optional(table_, out_, exp.slot);
  out_ << ", ";
  print(table_, out_, *exp.body);
  out_ << ", ";
  print(table_, out_, *exp.rest);
  out_ << " }";
}

void PrintExpVisitor::operator()(const JumpExp &exp) {
  out_ << "JumpExp { " << table_.lookup(exp.joinName) << ", ";
  print_optional(table_, out_, exp.slotValue);
  out_ << " }";
}

void PrintExpVisitor::operator()(const AppExp &exp) {
  out_ << "AppExp { " << table_.lookup(exp.name) << ", "
       << table_.lookup(exp.funName) << ", ";
  print_vector(table_, out_, exp.paramValues);
  out_ << ", ";
  print(table_, out_, *exp.rest);
  out_ << " }";
}

void PrintExpVisitor::operator()(const BopExp &exp) {
  std::string bop = binOpString(exp.bop);
  out_ << "BopExp { " << table_.lookup(exp.name) << ", " << bop << ", ";
  print(table_, out_, exp.param1);
  out_ << ", ";
  print(table_, out_, exp.param2);
  out_ << ", ";
  print(table_, out_, *exp.rest);
  out_ << " }";
}

void PrintExpVisitor::operator()(const TupleExp &exp) {
  out_ << "TupleExp { " << table_.lookup(exp.name) << ", ";
  print_vector(table_, out_, exp.values);
  out_ << ", ";
  print(table_, out_, *exp.rest);
  out_ << " }";
}

void PrintExpVisitor::operator()(const ProjExp &exp) {
  out_ << "ProjExp { " << table_.lookup(exp.name) << ", "
       << table_.lookup(exp.tuple) << ", " << exp.index << ", ";
  print(table_, out_, *exp.rest);
  out_ << " }";
}

void PrintExpVisitor::operator()(const IfExp &exp) {
  out_ << "IfExp { ";
  print(table_, out_, exp.cond);
  out_ << ", ";
  print(table_, out_, *exp.thenBranch);
  out_ << ", ";
  print(table_, out_, *exp.elseBranch);
  out_ << " }";
}

} // namespace anf
} // namespace lambcalc
#include "visitor.h"
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

template <typename T>
void print_vector(std::ostream &os, const std::vector<T> &vec) {
  os << "[";
  for (auto it = vec.begin(); it != vec.end(); ++it) {
    os << *it;
    if (it + 1 != vec.end()) {
      os << ", ";
    }
  }
  os << "]";
}

template <typename T>
void print_optional(std::ostream &os, std::optional<T> opt) {
  if (opt) {
    os << "<" << *opt << ">";
  } else {
    os << "<>";
  }
}

namespace ast {

template <template <class> class Ptr>
std::ostream &operator<<(std::ostream &os, const Exp<Ptr> &exp) {
  std::visit(PrintExpVisitor(os), exp);
  return os;
}

void PrintExpVisitor::operator()(const IntExp &exp) { out_ << exp.value; }
void PrintExpVisitor::operator()(const VarExp &exp) { out_ << exp.name; }
void PrintExpVisitor::operator()(const LamExp &exp) {
  out_ << "(fn " << exp.param << " => " << *exp.body << ")";
}
void PrintExpVisitor::operator()(const AppExp &exp) {
  out_ << "(" << *exp.fn << " " << *exp.arg << ")";
}
void PrintExpVisitor::operator()(const BopExp &exp) {
  out_ << "(" << *exp.arg1 << " " << binOpString(exp.bop) << " " << *exp.arg2
       << ")";
}
void PrintExpVisitor::operator()(const IfExp &exp) {
  out_ << "(if " << *exp.cond << " then " << *exp.then << " else " << *exp.els
       << ")";
}

} // namespace ast

namespace anf {

void PrintValueVisitor::operator()(const IntValue &value) {
  out_ << value.value;
}
void PrintValueVisitor::operator()(const VarValue &value) { out_ << value.var; }
void PrintValueVisitor::operator()(const GlobValue &value) {
  out_ << value.glob;
}

std::ostream &operator<<(std::ostream &os, const Value &value) {
  std::visit(PrintValueVisitor(os), value);
  return os;
}

void PrintExpVisitor::operator()(const HaltExp &exp) {
  out_ << "HaltExp { " << exp.value << " }";
}

void PrintExpVisitor::operator()(const FunExp &exp) {
  out_ << "FunExp { " << exp.name << ", ";
  print_vector(out_, exp.params);
  out_ << ", " << *exp.body << ", " << *exp.rest << " }";
}

void PrintExpVisitor::operator()(const JoinExp &exp) {
  out_ << "JoinExp { " << exp.name << ", ";
  print_optional(out_, exp.slot);
  out_ << ", " << *exp.body << ", " << *exp.rest << " }";
}

void PrintExpVisitor::operator()(const JumpExp &exp) {
  out_ << "JumpExp { " << exp.joinName << ", ";
  print_optional(out_, exp.slotValue);
  out_ << " }";
}

void PrintExpVisitor::operator()(const AppExp &exp) {
  out_ << "AppExp { " << exp.name << ", " << exp.funName << ", ";
  print_vector(out_, exp.paramValues);
  out_ << ", " << *exp.rest << " }";
}

void PrintExpVisitor::operator()(const BopExp &exp) {
  std::string bop = binOpString(exp.bop);
  out_ << "BopExp { " << exp.name << ", " << bop << ", " << exp.param1 << ", "
       << exp.param2 << ", " << *exp.rest << " }";
}

void PrintExpVisitor::operator()(const TupleExp &exp) {
  out_ << "TupleExp { " << exp.name << ", ";
  print_vector(out_, exp.values);
  out_ << ", " << *exp.rest << " }";
}

void PrintExpVisitor::operator()(const ProjExp &exp) {
  out_ << "ProjExp { " << exp.name << ", " << exp.tuple << ", " << exp.index
       << ", " << *exp.rest << " }";
}

void PrintExpVisitor::operator()(const IfExp &exp) {
  out_ << "IfExp { " << exp.cond << ", " << *exp.thenBranch << ", "
       << *exp.elseBranch << " }";
}

std::ostream &operator<<(std::ostream &os, const Exp &exp) {
  std::visit(PrintExpVisitor(os), exp);
  return os;
}

} // namespace anf
} // namespace lambcalc
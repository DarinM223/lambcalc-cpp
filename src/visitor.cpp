#include "visitor.h"
namespace lambcalc {
namespace anf {

template <typename Visitor, Task T, Worklist<T> W>
void WorklistVisitor<Visitor, T, W>::addWorklist(
    std::unique_ptr<Exp> *parentLink, Exp &exp) {
  worklist.push_back(T(parentLink, exp));
}

template <typename Visitor, Task T, Worklist<T> W>
decltype(auto) WorklistVisitor<Visitor, T, W>::operator()(HaltExp &exp) {
  return Visitor::operator()(exp);
}

template <typename Visitor, Task T, Worklist<T> W>
decltype(auto) WorklistVisitor<Visitor, T, W>::operator()(FunExp &exp) {
  auto result = Visitor::operator()(exp);
  addWorklist(&exp.body, *exp.body);
  addWorklist(&exp.rest, *exp.rest);
  return result;
}

template <typename Visitor, Task T, Worklist<T> W>
decltype(auto) WorklistVisitor<Visitor, T, W>::operator()(JoinExp &exp) {
  auto result = Visitor::operator()(exp);
  addWorklist(&exp.body, *exp.body);
  addWorklist(&exp.rest, *exp.rest);
  return result;
}

template <typename Visitor, Task T, Worklist<T> W>
decltype(auto) WorklistVisitor<Visitor, T, W>::operator()(JumpExp &exp) {
  return Visitor::operator()(exp);
}

template <typename Visitor, Task T, Worklist<T> W>
decltype(auto) WorklistVisitor<Visitor, T, W>::operator()(AppExp &exp) {
  auto result = Visitor::operator()(exp);
  addWorklist(&exp.rest, *exp.rest);
  return result;
}

template <typename Visitor, Task T, Worklist<T> W>
decltype(auto) WorklistVisitor<Visitor, T, W>::operator()(BopExp &exp) {
  auto result = Visitor::operator()(exp);
  addWorklist(&exp.rest, *exp.rest);
  return result;
}

template <typename Visitor, Task T, Worklist<T> W>
decltype(auto) WorklistVisitor<Visitor, T, W>::operator()(IfExp &exp) {
  auto result = Visitor::operator()(exp);
  addWorklist(&exp.thenBranch, *exp.thenBranch);
  addWorklist(&exp.elseBranch, *exp.elseBranch);
  return result;
}

template <typename Visitor, Task T, Worklist<T> W>
decltype(auto) WorklistVisitor<Visitor, T, W>::operator()(TupleExp &exp) {
  auto result = Visitor::operator()(exp);
  addWorklist(&exp.rest, *exp.rest);
  return result;
}

template <typename Visitor, Task T, Worklist<T> W>
decltype(auto) WorklistVisitor<Visitor, T, W>::operator()(ProjExp &exp) {
  auto result = Visitor::operator()(exp);
  addWorklist(&exp.rest, *exp.rest);
  return result;
}

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

template <typename T> void print_vector(std::ostream &os, std::vector<T> vec) {
  os << "[";
  for (auto it = vec.begin(); it != vec.end(); ++it) {
    os << *it;
    if (it != vec.end()) {
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

void PrintExpVisitor::operator()(const HaltExp &exp) {
  out_ << "HaltExp { " << exp.value << " }";
}

void PrintExpVisitor::operator()(const FunExp &exp) {
  out_ << "FunExp { " << exp.name << ", ";
  print_vector(out_, exp.params);
  out_ << ", " << *exp.body << ", " << *exp.rest << " }";
}

void PrintExpVisitor::operator()(const JoinExp &exp) {
  out_ << "JoinExp { ";
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
  out_ << "BopExp { " << exp.name << ", " << static_cast<int>(exp.bop) << ", "
       << exp.param1 << ", " << exp.param2 << ", " << *exp.rest << " }";
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
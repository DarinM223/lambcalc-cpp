#ifndef VISITOR_H
#define VISITOR_H

#include "anf.h"
#include <deque>

namespace lambcalc {
namespace anf {

template <typename T, typename Task>
concept Worklist =
    requires(T worklist, Task task) { worklist.push_back(task); };

template <typename T>
concept Task = requires(T task, std::unique_ptr<Exp> *parentLink, Exp &exp) {
  { T(parentLink, exp) } -> std::same_as<T>;
};

template <typename Visitor, Task T, Worklist<T> W>
class WorklistVisitor : public Visitor {
  W worklist;

public:
  template <class... Args> WorklistVisitor(Args... args) : Visitor(args...) {}
  void addWorklist(std::unique_ptr<Exp> *parentLink, Exp &exp);
  decltype(auto) operator()(HaltExp &exp);
  decltype(auto) operator()(FunExp &exp);
  decltype(auto) operator()(JoinExp &exp);
  decltype(auto) operator()(JumpExp &exp);
  decltype(auto) operator()(AppExp &exp);
  decltype(auto) operator()(BopExp &exp);
  decltype(auto) operator()(IfExp &exp);
  decltype(auto) operator()(TupleExp &exp);
  decltype(auto) operator()(ProjExp &exp);
};

class PrintValueVisitor {
  std::ostream &out_;

public:
  PrintValueVisitor(std::ostream &out) : out_(out) {}
  void operator()(const IntValue &value);
  void operator()(const VarValue &value);
  void operator()(const GlobValue &value);
};

class PrintExpVisitor {
  std::ostream &out_;

public:
  PrintExpVisitor(std::reference_wrapper<std::ostream> out) : out_(out) {}
  void operator()(const HaltExp &exp);
  void operator()(const FunExp &exp);
  void operator()(const JoinExp &exp);
  void operator()(const JumpExp &exp);
  void operator()(const AppExp &exp);
  void operator()(const BopExp &exp);
  void operator()(const IfExp &exp);
  void operator()(const TupleExp &exp);
  void operator()(const ProjExp &exp);
};

struct WorklistTask {
  std::unique_ptr<Exp> *parentLink;
  Exp &exp;
  std::optional<std::function<void(void)>> initializer;
  std::optional<std::function<void(void)>> finalizer;
};

// TODO: this visitor is unnecessary because PrintExpVisitor is already
// recursive, but this is an example of a general use case for a worklist.
struct PrintWorklistVisitor
    : public WorklistVisitor<PrintExpVisitor, WorklistTask,
                             std::deque<WorklistTask>> {
  PrintWorklistVisitor(std::reference_wrapper<std::ostream> out)
      : WorklistVisitor(out) {}
};

// Templates for making a nested pack, which does the transformation
// <T, A, B, C> -> A<B<C<T>>>. This could be useful for defining a pipeline
// of pattern matching for instruction selection where A, B, and C are sorted
// at compile time based on their constexpr cost.

template <class T, template <class> class... Types> struct NestedPackHelper;

template <class T> struct NestedPackHelper<T> {
  typedef T type;
};

template <class T, template <class> class Head, template <class> class... Tail>
struct NestedPackHelper<T, Head, Tail...> {
  typedef Head<typename NestedPackHelper<T, Tail...>::type> type;
};

template <class T, template <class> class... Types>
using NestedPack = typename NestedPackHelper<T, Types...>::type;

template <class T>
using VecDequeVec = NestedPack<T, std::vector, std::deque, std::vector>;

// compiles!
VecDequeVec<int> makeResult() {
  return std::vector{std::deque{std::vector{1}}};
}

} // namespace anf
} // namespace lambcalc

#endif
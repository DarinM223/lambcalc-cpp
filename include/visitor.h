#ifndef VISITOR_H
#define VISITOR_H

#include "anf.h"
#include <stack>

namespace lambcalc {
namespace anf {

template <typename T, typename Task>
concept Worklist = requires(T worklist, Task task) { worklist.push(task); };

template <typename T>
concept Task = requires(T task, std::unique_ptr<Exp> *parentLink, Exp &exp) {
  { T(parentLink, exp) } -> std::same_as<T>;
};

struct DefaultExpVisitor {
  void operator()(auto &) {}
};

template <typename Visitor, Task T, Worklist<T> W>
class WorklistVisitor : public Visitor {
  W worklist;

public:
  template <class... Args> WorklistVisitor(Args... args) : Visitor(args...) {}
  W &getWorklist() { return worklist; }
  virtual void addWorklist(std::unique_ptr<Exp> *parentLink, Exp &exp) {
    worklist.push(T(parentLink, exp));
  }
  virtual void addWorklist(const Var &name, std::unique_ptr<Exp> *parentLink,
                           Exp &exp) {
    addWorklist(parentLink, exp);
  }
  virtual void addWorklist(const std::vector<Var> &name,
                           std::unique_ptr<Exp> *parentLink, Exp &exp) {
    addWorklist(parentLink, exp);
  }

  using RetTy =
      decltype(std::declval<Visitor>().operator()(std::declval<HaltExp &>()));

#define DISPATCH(GO)                                                           \
  if constexpr (std::is_same_v<RetTy, void>) {                                 \
    GO return Visitor::operator()(exp);                                        \
  } else {                                                                     \
    auto result = Visitor::operator()(exp);                                    \
    GO return result;                                                          \
  }

  decltype(auto) operator()(HaltExp &exp) { return Visitor::operator()(exp); }
  decltype(auto) operator()(FunExp &exp) {
    DISPATCH(addWorklist(exp.params, &exp.body, *exp.body);
             addWorklist(exp.name, &exp.rest, *exp.rest);)
  }
  decltype(auto) operator()(JoinExp &exp) {
    DISPATCH({
      if (exp.slot) {
        addWorklist(*exp.slot, &exp.body, *exp.body);
      } else {
        addWorklist(&exp.body, *exp.body);
      }
      addWorklist(exp.name, &exp.rest, *exp.rest);
    })
  }
  decltype(auto) operator()(JumpExp &exp) { return Visitor::operator()(exp); }
  decltype(auto) operator()(AppExp &exp) {
    DISPATCH(addWorklist(exp.name, &exp.rest, *exp.rest);)
  }
  decltype(auto) operator()(BopExp &exp) {
    DISPATCH(addWorklist(exp.name, &exp.rest, *exp.rest);)
  }
  decltype(auto) operator()(IfExp &exp) {
    DISPATCH(addWorklist(&exp.thenBranch, *exp.thenBranch);
             addWorklist(&exp.elseBranch, *exp.elseBranch);)
  }
  decltype(auto) operator()(TupleExp &exp) {
    DISPATCH(addWorklist(exp.name, &exp.rest, *exp.rest);)
  }
  decltype(auto) operator()(ProjExp &exp) {
    DISPATCH(addWorklist(exp.name, &exp.rest, *exp.rest);)
  }
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

using NodeTask = std::pair<std::unique_ptr<Exp> *, std::reference_wrapper<Exp>>;
using FnTask = std::function<void(void)>;

struct WorklistTask : std::variant<NodeTask, FnTask> {
  using variant::variant;
  WorklistTask(std::unique_ptr<Exp> *parentLink,
               std::reference_wrapper<Exp> exp)
      : WorklistTask(std::in_place_index<0>, parentLink, exp) {}
};

// TODO: this visitor is unnecessary because PrintExpVisitor is already
// recursive, but this is an example of a general use case for a worklist.
struct PrintWorklistVisitor
    : public WorklistVisitor<PrintExpVisitor, WorklistTask,
                             std::stack<WorklistTask>> {
  PrintWorklistVisitor(std::reference_wrapper<std::ostream> out)
      : WorklistVisitor(out) {}
};

} // namespace anf
} // namespace lambcalc

#endif
#ifndef VISITOR_H
#define VISITOR_H

#include "anf.h"
#include <stack>

namespace lambcalc {

namespace ast {

class PrintExpVisitor {
  std::ostream &out_;

public:
  PrintExpVisitor(std::reference_wrapper<std::ostream> out) : out_(out) {}
  void operator()(const IntExp &exp);
  void operator()(const VarExp &exp);
  void operator()(const LamExp &exp);
  void operator()(const AppExp &exp);
  void operator()(const BopExp &exp);
  void operator()(const IfExp &exp);
};

} // namespace ast

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

template <typename Visitor> struct MatchIfJump : public Visitor {
  using RetTy =
      decltype(std::declval<Visitor>().operator()(std::declval<IfExp &>()));
  using Visitor::operator();
  template <class... Args> MatchIfJump(Args... args) : Visitor(args...) {}
  RetTy operator()(IfExp &exp) {
    JumpExp *thenJump, *elseJump;
    if ((thenJump = std::get_if<JumpExp>(&*exp.thenBranch)) &&
        (elseJump = std::get_if<JumpExp>(&*exp.elseBranch))) {
      return visitIfJump(exp, *thenJump, *elseJump);
    }
    return Visitor::operator()(exp);
  }
  virtual RetTy visitIfJump(IfExp &exp, JumpExp &thenJump,
                            JumpExp &elseJump) = 0;
};

template <typename Visitor, Task T, template <class> class W>
  requires Worklist<W<T>, T>
class WorklistVisitor : public Visitor {
  W<T> worklist;

public:
  template <class... Args> WorklistVisitor(Args... args) : Visitor(args...) {}
  W<T> &getWorklist() { return worklist; }
  virtual void addWorklist(std::unique_ptr<Exp> *parentLink, Exp &exp) {
    worklist.push(T(parentLink, exp));
  }
  virtual void addWorklist(const Var &name, std::unique_ptr<Exp> *parentLink,
                           Exp &exp) {
    addWorklist(parentLink, exp);
  }
  virtual void addWorklist(const std::vector<Var> &names,
                           std::unique_ptr<Exp> *parentLink, Exp &exp) {
    addWorklist(parentLink, exp);
  }

  using RetTy =
      decltype(std::declval<Visitor>().operator()(std::declval<HaltExp &>()));

#define DISPATCH(GO)                                                           \
  if constexpr (std::is_same_v<RetTy, void>) {                                 \
    Visitor::operator()(exp);                                                  \
    GO                                                                         \
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

template <typename Visitor> class ExpValueVisitor;

template <typename Visitor> struct ValueVisitor {
  ExpValueVisitor<Visitor> *parent_;
  void operator()(IntValue &value) { parent_->visitIntValue(value); }
  void operator()(VarValue &value) { parent_->visitVarValue(value); }
  void operator()(GlobValue &value) { parent_->visitGlobValue(value); }
};

template <typename Visitor> class ExpValueVisitor : public Visitor {
  ValueVisitor<Visitor> valueVisitor_;

public:
  template <class... Args>
  ExpValueVisitor(Args... args) : Visitor(args...), valueVisitor_(this) {}
  virtual void visitIntValue(IntValue &value) {}
  virtual void visitVarValue(VarValue &value) {}
  virtual void visitGlobValue(GlobValue &value) {}
  void visitValue(Value &value) { std::visit(valueVisitor_, value); }

  decltype(auto) operator()(HaltExp &exp) {
    visitValue(exp.value);
    return Visitor::operator()(exp);
  }
  decltype(auto) operator()(FunExp &exp) { return Visitor::operator()(exp); }
  decltype(auto) operator()(JoinExp &exp) { return Visitor::operator()(exp); }
  decltype(auto) operator()(JumpExp &exp) {
    if (exp.slotValue) {
      visitValue(*exp.slotValue);
    }
    return Visitor::operator()(exp);
  }
  decltype(auto) operator()(AppExp &exp) {
    Value funValue(std::in_place_type<VarValue>, exp.funName);
    visitValue(funValue);
    for (auto &v : exp.paramValues) {
      visitValue(v);
    }
    return Visitor::operator()(exp);
  }
  decltype(auto) operator()(BopExp &exp) {
    visitValue(exp.param1);
    visitValue(exp.param2);
    return Visitor::operator()(exp);
  }
  decltype(auto) operator()(IfExp &exp) {
    visitValue(exp.cond);
    return Visitor::operator()(exp);
  }
  decltype(auto) operator()(TupleExp &exp) {
    for (auto &v : exp.values) {
      visitValue(v);
    }
    return Visitor::operator()(exp);
  }
  decltype(auto) operator()(ProjExp &exp) {
    Value tupleValue(std::in_place_type<VarValue>, exp.tuple);
    visitValue(tupleValue);
    return Visitor::operator()(exp);
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
using FnTask = std::move_only_function<void(void)>;

struct WorklistTask : std::variant<NodeTask, FnTask> {
  using variant::variant;
  WorklistTask(std::unique_ptr<Exp> *parentLink,
               std::reference_wrapper<Exp> exp)
      : WorklistTask(std::in_place_index<0>, parentLink, exp) {}
};

// TODO: this visitor is unnecessary because PrintExpVisitor is already
// recursive, but this is an example of a general use case for a worklist.
struct PrintWorklistVisitor
    : public WorklistVisitor<PrintExpVisitor, WorklistTask, std::stack> {
  PrintWorklistVisitor(std::reference_wrapper<std::ostream> out)
      : WorklistVisitor(out) {}
};

} // namespace anf
} // namespace lambcalc

#endif
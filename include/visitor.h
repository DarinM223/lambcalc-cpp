#ifndef VISITOR_H
#define VISITOR_H

#include "anf.h"
#include <stack>
#include <variant>

namespace lambcalc {

#define DISPATCH(GO)                                                           \
  if constexpr (std::is_same_v<RetTy, void>) {                                 \
    Visitor::operator()(exp);                                                  \
    GO                                                                         \
  } else {                                                                     \
    auto result = Visitor::operator()(exp);                                    \
    GO return result;                                                          \
  }

struct DefaultVisitor {
  void operator()(auto &) {}
};

template <typename Exp>
using NodeTask = std::pair<std::unique_ptr<Exp> *, std::reference_wrapper<Exp>>;
using FnTask = std::move_only_function<void(void)>;

template <typename Exp>
struct WorklistTask : std::variant<NodeTask<Exp>, FnTask> {
  using std::variant<NodeTask<Exp>, FnTask>::variant;
  explicit WorklistTask(std::unique_ptr<Exp> *parentLink)
      : WorklistTask(std::in_place_index<0>, parentLink, **parentLink) {}
};

template <typename Exp> struct DestructorTask {
  std::unique_ptr<Exp> exp;
  explicit DestructorTask(std::unique_ptr<Exp> *parentLink)
      : exp(parentLink == nullptr ? nullptr : std::move(*parentLink)) {}
};

template <typename T, typename Task>
concept Worklist = requires(T worklist, Task task) { worklist.push(task); };

namespace ast {

template <typename T>
concept Task = requires(T task, std::unique_ptr<Exp<>> *parentLink) {
  { T(parentLink) } -> std::same_as<T>;
};

template <template <class> class Ptr> class PrintExpVisitor {
  std::ostream &out_;

public:
  explicit PrintExpVisitor(std::reference_wrapper<std::ostream> out)
      : out_(out) {}
  void operator()(const IntExp &exp);
  void operator()(const VarExp &exp);
  void operator()(const LamExp<Ptr> &exp);
  void operator()(const AppExp<Ptr> &exp);
  void operator()(const BopExp<Ptr> &exp);
  void operator()(const IfExp<Ptr> &exp);
};

template <typename Visitor, Task T, template <class> class W>
  requires Worklist<W<T>, T>
class WorklistVisitor : public Visitor {
  W<T> worklist;

public:
  template <class... Args> WorklistVisitor(Args... args) : Visitor(args...) {}
  W<T> &getWorklist() { return worklist; }
  virtual void addWorklist(std::unique_ptr<Exp<>> *parentLink) {
    worklist.push(T(parentLink));
  }
  virtual void addWorklist(const std::string &,
                           std::unique_ptr<Exp<>> *parentLink) {
    addWorklist(parentLink);
  }

  using RetTy =
      decltype(std::declval<Visitor>().operator()(std::declval<IntExp &>()));

  decltype(auto) operator()(IntExp &exp) { return Visitor::operator()(exp); }
  decltype(auto) operator()(VarExp &exp) { return Visitor::operator()(exp); }
  decltype(auto) operator()(LamExp<std::unique_ptr> &exp) {
    DISPATCH(addWorklist(exp.param, &exp.body);)
  }
  decltype(auto) operator()(AppExp<std::unique_ptr> &exp) {
    DISPATCH(addWorklist(&exp.fn); addWorklist(&exp.arg);)
  }
  decltype(auto) operator()(BopExp<std::unique_ptr> &exp) {
    DISPATCH(addWorklist(&exp.arg1); addWorklist(&exp.arg2);)
  }
  decltype(auto) operator()(IfExp<std::unique_ptr> &exp) {
    DISPATCH(addWorklist(&exp.cond); addWorklist(&exp.then);
             addWorklist(&exp.els);)
  }
};

} // namespace ast

namespace anf {

template <typename T>
concept Task = requires(T task, std::unique_ptr<Exp> *parentLink) {
  { T(parentLink) } -> std::same_as<T>;
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
  virtual void addWorklist(std::unique_ptr<Exp> *parentLink) {
    worklist.push(T(parentLink));
  }
  virtual void addWorklist(const Var &, std::unique_ptr<Exp> *parentLink) {
    addWorklist(parentLink);
  }
  virtual void addWorklist(const std::vector<Var> &,
                           std::unique_ptr<Exp> *parentLink) {
    addWorklist(parentLink);
  }

  using RetTy =
      decltype(std::declval<Visitor>().operator()(std::declval<HaltExp &>()));

  decltype(auto) operator()(HaltExp &exp) { return Visitor::operator()(exp); }
  decltype(auto) operator()(FunExp &exp) {
    DISPATCH(addWorklist(exp.params, &exp.body);
             addWorklist(exp.name, &exp.rest);)
  }
  decltype(auto) operator()(JoinExp &exp) {
    DISPATCH({
      if (exp.slot) {
        addWorklist(*exp.slot, &exp.body);
      } else {
        addWorklist(&exp.body);
      }
      addWorklist(exp.name, &exp.rest);
    })
  }
  decltype(auto) operator()(JumpExp &exp) { return Visitor::operator()(exp); }
  decltype(auto) operator()(AppExp &exp) {
    DISPATCH(addWorklist(exp.name, &exp.rest);)
  }
  decltype(auto) operator()(BopExp &exp) {
    DISPATCH(addWorklist(exp.name, &exp.rest);)
  }
  decltype(auto) operator()(IfExp &exp) {
    DISPATCH(addWorklist(&exp.thenBranch); addWorklist(&exp.elseBranch);)
  }
  decltype(auto) operator()(TupleExp &exp) {
    DISPATCH(addWorklist(exp.name, &exp.rest);)
  }
  decltype(auto) operator()(ProjExp &exp) {
    DISPATCH(addWorklist(exp.name, &exp.rest);)
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
  virtual void visitIntValue(IntValue &) {}
  virtual void visitVarValue(VarValue &) {}
  virtual void visitGlobValue(GlobValue &) {}
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
  explicit PrintValueVisitor(std::ostream &out) : out_(out) {}
  void operator()(const IntValue &value);
  void operator()(const VarValue &value);
  void operator()(const GlobValue &value);
};

class PrintExpVisitor {
  std::ostream &out_;

public:
  explicit PrintExpVisitor(std::reference_wrapper<std::ostream> out)
      : out_(out) {}
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

} // namespace anf
} // namespace lambcalc

#endif
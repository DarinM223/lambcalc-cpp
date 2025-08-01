#include "convert.h"
#include "utils.h"
#include "visitor.h"
#include <cassert>

namespace lambcalc {
namespace convert {
using namespace anf;

using FreeVarsPipeline = WorklistVisitor<ExpValueVisitor<DefaultExpVisitor>,
                                         WorklistTask, std::stack>;
class FreeVarsVisitor : public FreeVarsPipeline {
  std::set<Var> &freeVars_;

public:
  FreeVarsVisitor(std::set<Var> &freeVars) : freeVars_(freeVars) {}
  using FreeVarsPipeline::operator();
  using FreeVarsPipeline::addWorklist;

  void addWorklist(const Var &name, std::unique_ptr<Exp> *parentLink,
                   Exp &exp) override {
    getWorklist().emplace(std::in_place_index<1>,
                          [&]() { freeVars_.erase(name); });
    addWorklist(parentLink, exp);
  }
  void addWorklist(const std::vector<Var> &names,
                   std::unique_ptr<Exp> *parentLink, Exp &exp) override {
    getWorklist().emplace(std::in_place_index<1>, [&]() {
      for (auto &v : names) {
        freeVars_.erase(v);
      }
    });
    addWorklist(parentLink, exp);
  }

  void visitVarValue(VarValue &value) override { freeVars_.insert(value.var); }
  void visitGlobValue(GlobValue &value) override {
    freeVars_.insert(value.glob);
  }
};

using ClosureConvertPipeline =
    WorklistVisitor<DefaultExpVisitor, WorklistTask, std::stack>;
class ClosureConvertVisitor : public ClosureConvertPipeline {
  int counter_;
  std::unique_ptr<Exp> *parentLink_;
  Var fresh(std::string_view prefix) {
    return prefix.data() + std::to_string(counter_++);
  }

public:
  using ClosureConvertPipeline::operator();
  ClosureConvertVisitor() : counter_(0), parentLink_(nullptr) {}
  void setParent(std::unique_ptr<Exp> *parentLink) { parentLink_ = parentLink; }
  void operator()(FunExp &exp) {
    assert(parentLink_ != nullptr &&
           "Expected FunExp to have a unique_ptr link");
    auto freeVariables = freeVars(**parentLink_);
    auto closureParam = fresh("closure");
    exp.params.insert(exp.params.begin(), closureParam);
    auto body = std::move(exp.body);
    int i = 1;
    for (auto var : freeVariables) {
      body = make(ProjExp{var, closureParam, i++, std::move(body)});
    }
    exp.body = std::move(body);

    std::vector<Value> vars;
    vars.push_back(GlobValue{exp.name});
    for (auto var : freeVariables) {
      vars.push_back(VarValue{std::move(var)});
    }
    exp.rest = make(TupleExp{exp.name, vars, std::move(exp.rest)});
    ClosureConvertPipeline::operator()(exp);
  }
  void operator()(AppExp &exp) {
    assert(parentLink_ != nullptr &&
           "Expected AppExp to have a unique_ptr link");
    auto projName = fresh("proj");
    auto paramValues = std::move(exp.paramValues);
    paramValues.insert(paramValues.begin(), VarValue{exp.funName});

    auto parentLink = parentLink_;
    getWorklist().emplace(
        std::in_place_index<1>,
        [&exp, projName = std::move(projName),
         paramValues = std::move(paramValues), parentLink]() {
          auto appData = AppExp{std::move(exp.name), projName,
                                std::move(paramValues), std::move(exp.rest)};
          auto app = make(std::move(appData));
          *parentLink = make(ProjExp{projName, exp.funName, 0, std::move(app)});
        });
    addWorklist(&exp.rest, *exp.rest);
  }
};

std::set<Var> freeVars(Exp &root) {
  std::set<Var> freeVars;
  FreeVarsVisitor visitor(freeVars);
  auto &worklist = visitor.getWorklist();
  worklist.emplace(nullptr, root);
  while (!worklist.empty()) {
    auto task = std::move(worklist.top());
    worklist.pop();
    std::visit(overloaded{[&](NodeTask &n) {
                            Exp &exp = std::get<1>(n);
                            std::visit(visitor, exp);
                          },
                          [](FnTask &f) { f(); }},
               task);
  }
  return freeVars;
}

std::unique_ptr<Exp> closureConvert(std::unique_ptr<Exp> &&start) {
  ClosureConvertVisitor visitor;
  auto &worklist = visitor.getWorklist();
  auto root = std::move(start);
  worklist.emplace(&root, *root);
  while (!worklist.empty()) {
    auto task = std::move(worklist.top());
    worklist.pop();
    std::visit(overloaded{[&](NodeTask &n) {
                            auto [parent, exp] = n;
                            visitor.setParent(parent);
                            std::visit(visitor, static_cast<Exp &>(exp));
                          },
                          [](FnTask &f) { f(); }},
               task);
  }
  return root;
}

} // namespace convert
} // namespace lambcalc
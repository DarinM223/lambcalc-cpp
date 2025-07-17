#include "convert.h"
#include "utils.h"
#include "visitor.h"

namespace lambcalc {
namespace convert {
using namespace anf;

using Pipeline = WorklistVisitor<ExpValueVisitor<DefaultExpVisitor>,
                                 WorklistTask, std::stack>;
class FreeVarsVisitor : public Pipeline {
  std::set<Var> &freeVars;

public:
  FreeVarsVisitor(std::set<Var> &freeVars) : freeVars(freeVars) {}
  using Pipeline::operator();
  using Pipeline::addWorklist;

  void addWorklist(const Var &name, std::unique_ptr<Exp> *parentLink,
                   Exp &exp) override {
    getWorklist().emplace(std::in_place_index<1>,
                          [&]() { freeVars.erase(name); });
    addWorklist(parentLink, exp);
  }
  void addWorklist(const std::vector<Var> &names,
                   std::unique_ptr<Exp> *parentLink, Exp &exp) override {
    getWorklist().emplace(std::in_place_index<1>, [&]() {
      for (auto &v : names) {
        freeVars.erase(v);
      }
    });
    addWorklist(parentLink, exp);
  }

  void visitValue(VarValue &value) override { freeVars.insert(value.var); }
  void visitValue(GlobValue &value) override { freeVars.insert(value.glob); }
};

std::set<Var> freeVars(Exp &root) {
  std::set<Var> freeVars;
  FreeVarsVisitor visitor(freeVars);
  auto &worklist = visitor.getWorklist();
  worklist.emplace(nullptr, root);
  while (!worklist.empty()) {
    WorklistTask task = worklist.top();
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

} // namespace convert
} // namespace lambcalc
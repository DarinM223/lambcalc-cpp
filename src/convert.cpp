#include "convert.h"
#include "utils.h"
#include "visitor.h"

namespace lambcalc {
namespace convert {
using namespace anf;

struct FreeVarsVisitor : public WorklistVisitor<DefaultExpVisitor, WorklistTask,
                                                std::stack<WorklistTask>> {
  std::set<Var> freeVars;
  using WorklistVisitor<DefaultExpVisitor, WorklistTask,
                        std::stack<WorklistTask>>::operator();
};

std::set<Var> freeVars(Exp &root) {
  FreeVarsVisitor visitor{};
  auto worklist = visitor.getWorklist();
  worklist.emplace(nullptr, root);
  while (!worklist.empty()) {
    WorklistTask task = worklist.top();
    worklist.pop();
    std::visit(overloaded{[&visitor = visitor](NodeTask &n) {
                            Exp &exp = std::get<1>(n);
                            std::visit(visitor, exp);
                          },
                          [](FnTask &f) { f(); }},
               task);
  }
  return {};
}

} // namespace convert
} // namespace lambcalc
#include "rename.h"
#include "utils.h"
#include "visitor.h"
#include <memory>

namespace lambcalc {
namespace ast {

using AlphaRenamePipeline =
    WorklistVisitor<DefaultVisitor, WorklistTask<Exp<>>, std::stack>;
class AlphaRenameVisitor : public AlphaRenamePipeline {
  int counter_;
  std::unordered_map<std::string, std::string> rename_;

  std::string fresh(std::string_view name) {
    return name.data() + std::to_string(counter_++);
  }

public:
  AlphaRenameVisitor() : counter_(0) {}
  using AlphaRenamePipeline::operator();
  void operator()(LamExp<std::unique_ptr> &exp) {
    auto renamed = fresh(exp.param);
    std::optional<std::string> oldRenameParam =
        rename_.contains(exp.param) ? std::optional{rename_[exp.param]}
                                    : std::nullopt;
    rename_[exp.param] = renamed;
    auto oldExpParam = std::move(exp.param);
    exp.param = std::move(renamed);
    getWorklist().emplace(std::in_place_index<1>,
                          [&, expParam = std::move(oldExpParam),
                           oldParam = std::move(oldRenameParam)]() {
                            if (oldParam) {
                              rename_[expParam] = *oldParam;
                            } else {
                              rename_.erase(expParam);
                            }
                          });
    AlphaRenamePipeline::operator()(exp);
  }
  void operator()(VarExp &exp) {
    if (rename_.contains(exp.name)) {
      exp.name = rename_[exp.name];
    } else {
      throw NotInScopeException(exp.name);
    }
  }
};

void rename(ast::Exp<> &exp) {
  AlphaRenameVisitor visitor;
  auto &worklist = visitor.getWorklist();
  std::visit(visitor, exp);
  while (!worklist.empty()) {
    auto task = std::move(worklist.top());
    worklist.pop();
    std::visit(overloaded{[&](NodeTask<Exp<>> &n) {
                            Exp<> &exp = std::get<1>(n);
                            std::visit(visitor, exp);
                          },
                          [](FnTask &f) { std::move(f)(); }},
               task);
  }
}

} // namespace ast
} // namespace lambcalc
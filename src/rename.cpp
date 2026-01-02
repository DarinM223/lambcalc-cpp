#include "rename.h"
#include "utils.h"
#include "visitor.h"
#include <stack>

namespace lambcalc {
namespace ast {

template <template <class> class Ptr>
using AlphaRenamePipeline =
    WorklistVisitor<DefaultVisitor, WorklistTask<Exp<Ptr>, Ptr>, std::stack,
                    Ptr>;
template <template <class> class Ptr>
class AlphaRenameVisitor : public AlphaRenamePipeline<Ptr> {
  int counter_;
  std::unordered_map<std::string, std::string> rename_;

  std::string fresh(std::string_view name) {
    return name.data() + std::to_string(counter_++);
  }

public:
  AlphaRenameVisitor() : counter_(0) {}
  using AlphaRenamePipeline<Ptr>::operator();
  using AlphaRenamePipeline<Ptr>::getWorklist;
  void operator()(LamExp<Ptr> &exp) {
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
    AlphaRenamePipeline<Ptr>::operator()(exp);
  }
  void operator()(VarExp &exp) {
    if (rename_.contains(exp.name)) {
      exp.name = rename_[exp.name];
    } else {
      throw NotInScopeException(exp.name);
    }
  }
};

template <template <class> class Ptr> void rename(ast::Exp<Ptr> &exp) {
  AlphaRenameVisitor<Ptr> visitor;
  auto &worklist = visitor.getWorklist();
  std::visit(visitor, exp);
  while (!worklist.empty()) {
    auto task = std::move(worklist.top());
    worklist.pop();
    std::visit(overloaded{[&](NodeTask<Exp<Ptr>, Ptr> &n) {
                            Exp<Ptr> &exp = std::get<1>(n);
                            std::visit(visitor, exp);
                          },
                          [](FnTask &f) { std::move(f)(); }},
               task);
  }
}

template void rename(ast::Exp<std::unique_ptr> &);
template void rename(ast::Exp<raw_ptr> &);

} // namespace ast
} // namespace lambcalc
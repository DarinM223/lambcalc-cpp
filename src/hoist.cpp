#include "hoist.h"
#include "utils.h"
#include "visitor.h"

namespace lambcalc {
namespace anf {

using HoistPipeline =
    WorklistVisitor<DefaultVisitor, WorklistTask<Exp>, std::stack>;
class HoistVisitor : public HoistPipeline {
  int counter_;
  std::vector<Join> currentJoins_;
  std::vector<Function> collected_;
  std::unique_ptr<Exp> *parentLink_;

  Var fresh(std::string_view prefix) {
    return prefix.data() + std::to_string(counter_++);
  }

public:
  using HoistPipeline::operator();
  HoistVisitor() : counter_(0), parentLink_(nullptr) {}
  void setParentLink(std::unique_ptr<Exp> *parentLink) {
    parentLink_ = parentLink;
  }
  std::vector<Function> moveOutCollectedFunctions() {
    return std::move(collected_);
  }
  void operator()(FunExp &exp) {
    std::vector<Join> savedJoins;
    std::swap(currentJoins_, savedJoins);
    // First save the current joins, recurse into body, then collect
    // the current joins into a function, then recurse into the rest.
    // NOTE: ordering is reversed since it is the order in which
    // tasks are pushed to the stack.
    auto savedParentLink = parentLink_;
    getWorklist().emplace(std::in_place_index<1>, [savedParentLink, &exp]() {
      if (savedParentLink)
        *savedParentLink = std::move(exp.rest);
    });
    addWorklist(&exp.rest);
    getWorklist().emplace(
        std::in_place_index<1>,
        [&, savedJoins = std::move(savedJoins)]() mutable {
          Join entryBlock{fresh("entry"), std::nullopt, std::move(exp.body)};
          collected_.emplace_back(std::move(exp.name), std::move(exp.params),
                                  std::move(entryBlock),
                                  std::move(currentJoins_));
          currentJoins_ = std::move(savedJoins);
        });
    addWorklist(&exp.body);
  }
  void operator()(JoinExp &exp) {
    auto savedParentLink = parentLink_;
    getWorklist().emplace(std::in_place_index<1>, [savedParentLink, &exp]() {
      if (savedParentLink)
        *savedParentLink = std::move(exp.rest);
    });
    addWorklist(&exp.rest);
    getWorklist().emplace(std::in_place_index<1>, [&]() {
      currentJoins_.emplace_back(std::move(exp.name), std::move(exp.slot),
                                 std::move(exp.body));
    });
    addWorklist(&exp.body);
  }
  void operator()(IfExp &exp) {
    getWorklist().emplace(std::in_place_index<1>, [&]() {
      auto thenBlockName = fresh("then");
      auto elseBlockName = fresh("else");
      currentJoins_.emplace_back(thenBlockName, std::nullopt,
                                 std::move(exp.thenBranch));
      currentJoins_.emplace_back(elseBlockName, std::nullopt,
                                 std::move(exp.elseBranch));
      exp.thenBranch = make(JumpExp{thenBlockName, std::nullopt});
      exp.elseBranch = make(JumpExp{elseBlockName, std::nullopt});
    });
    addWorklist(&exp.elseBranch);
    addWorklist(&exp.thenBranch);
  }
};

std::vector<Function> hoist(std::unique_ptr<anf::Exp> &&start) {
  HoistVisitor visitor;
  auto root =
      make(FunExp{"main", {}, std::move(start), make(HaltExp{IntValue{0}})});
  auto &worklist = visitor.getWorklist();
  worklist.emplace(&root);
  while (!worklist.empty()) {
    auto task = std::move(worklist.top());
    worklist.pop();
    std::visit(overloaded{[&](const NodeTask<Exp> &n) {
                            auto [parent, exp] = n;
                            visitor.setParentLink(parent);
                            std::visit(visitor, static_cast<Exp &>(exp));
                          },
                          [](FnTask &f) { std::move(f)(); }},
               task);
  }
  return visitor.moveOutCollectedFunctions();
}

} // namespace anf
} // namespace lambcalc

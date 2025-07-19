#include "hoist.h"
#include "utils.h"
#include "visitor.h"

namespace lambcalc {
namespace anf {

using HoistPipeline =
    WorklistVisitor<DefaultExpVisitor, WorklistTask, std::stack>;
class HoistVisitor : public HoistPipeline {
  int counter_;
  std::vector<Join> currentJoins;
  std::vector<Function> collected;
  std::unique_ptr<Exp> *parentLink;

  Var fresh(std::string_view prefix) {
    return prefix.data() + std::to_string(counter_++);
  }

public:
  using HoistPipeline::operator();
  HoistVisitor() : counter_(0) {}
  void setParentLink(std::unique_ptr<Exp> *parentLink) {
    this->parentLink = parentLink;
  }
  std::vector<Function> moveOutCollectedFunctions() {
    return std::move(collected);
  }
  void operator()(FunExp &exp) {
    std::vector<Join> savedJoins;
    std::swap(currentJoins, savedJoins);
    // First save the current joins, recurse into body, then collect
    // the current joins into a function, then recurse into the rest.
    // NOTE: ordering is reversed since it is the order in which
    // tasks are pushed to the stack.
    auto savedParentLink = parentLink;
    getWorklist().emplace(std::in_place_index<1>, [savedParentLink, &exp]() {
      if (savedParentLink)
        *savedParentLink = std::move(exp.rest);
    });
    addWorklist(&exp.rest, *exp.rest);
    getWorklist().emplace(
        std::in_place_index<1>,
        [&, savedJoins = std::move(savedJoins)]() mutable {
          Join entryBlock{fresh("entry"), std::nullopt, std::move(exp.body)};
          collected.emplace_back(std::move(exp.name), std::move(exp.params),
                                 std::move(entryBlock),
                                 std::move(currentJoins));
          currentJoins = std::move(savedJoins);
        });
    addWorklist(&exp.body, *exp.body);
  }
  void operator()(JoinExp &exp) {
    auto savedParentLink = parentLink;
    getWorklist().emplace(std::in_place_index<1>, [savedParentLink, &exp]() {
      if (savedParentLink)
        *savedParentLink = std::move(exp.rest);
    });
    addWorklist(&exp.rest, *exp.rest);
    getWorklist().emplace(std::in_place_index<1>, [&]() {
      currentJoins.emplace_back(std::move(exp.name), std::move(exp.slot),
                                std::move(exp.body));
    });
    addWorklist(&exp.body, *exp.body);
  }
  void operator()(IfExp &exp) {
    getWorklist().emplace(std::in_place_index<1>, [&]() {
      auto thenBlockName = fresh("then");
      auto elseBlockName = fresh("else");
      currentJoins.emplace_back(thenBlockName, std::nullopt,
                                std::move(exp.thenBranch));
      currentJoins.emplace_back(elseBlockName, std::nullopt,
                                std::move(exp.elseBranch));
      exp.thenBranch = make(JumpExp{thenBlockName, std::nullopt});
      exp.elseBranch = make(JumpExp{elseBlockName, std::nullopt});
    });
    addWorklist(&exp.elseBranch, *exp.elseBranch);
    addWorklist(&exp.thenBranch, *exp.thenBranch);
  }
};

std::vector<Function> hoist(std::unique_ptr<anf::Exp> &&start) {
  HoistVisitor visitor;
  auto root =
      make(FunExp{"main", {}, std::move(start), make(HaltExp{IntValue{0}})});
  auto &worklist = visitor.getWorklist();
  worklist.emplace(&root, *root);
  while (!worklist.empty()) {
    auto task = std::move(worklist.top());
    worklist.pop();
    std::visit(overloaded{[&](NodeTask &n) {
                            auto [parent, exp] = n;
                            visitor.setParentLink(parent);
                            std::visit(visitor, static_cast<Exp &>(exp));
                          },
                          [](FnTask &f) { f(); }},
               task);
  }
  return visitor.moveOutCollectedFunctions();
}

} // namespace anf
} // namespace lambcalc

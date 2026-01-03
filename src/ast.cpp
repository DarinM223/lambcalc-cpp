#include "ast.h"
#include "utils.h"
#include "visitor.h"
#include <concepts>
#include <memory>
#include <queue>
#include <sstream>

namespace lambcalc {
namespace ast {

std::unique_ptr<Exp<>> make(Exp<> &&exp) {
  return std::make_unique<Exp<>>(std::move(exp));
}

template <template <class> class Ptr> std::string Exp<Ptr>::dump() {
  std::ostringstream out;
  out << *this;
  return out.str();
}

struct DestructorVisitor
    : WorklistVisitor<DefaultVisitor, DestructorTask<Exp<>>, std::queue> {};

template <template <class> class Ptr> Exp<Ptr>::~Exp() {
  if constexpr (std::same_as<Ptr<int>, std::unique_ptr<int>>) {
    DestructorVisitor visitor;
    auto &queue = visitor.getWorklist();
    std::visit(visitor, *this);
    while (!queue.empty()) {
      auto task = std::move(queue.front());
      queue.pop();

      if (task.exp != nullptr) {
        std::visit(visitor, *task.exp);
      }
    }
  }
}

template struct Exp<std::unique_ptr>;
template struct Exp<raw_ptr>;

}; // namespace ast
} // namespace lambcalc
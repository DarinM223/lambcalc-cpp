#include "ast.h"
#include "visitor.h"
#include <queue>
#include <sstream>

namespace lambcalc {
namespace ast {

std::unique_ptr<Exp> make(Exp &&exp) {
  return std::make_unique<Exp>(std::move(exp));
}

std::string Exp::dump() {
  std::ostringstream out;
  out << *this;
  return out.str();
}

struct DestructorVisitor
    : WorklistVisitor<DefaultVisitor, DestructorTask<Exp>, std::queue> {};

Exp::~Exp() {
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

}; // namespace ast
} // namespace lambcalc
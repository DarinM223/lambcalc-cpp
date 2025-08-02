#include "rename.h"
#include "visitor.h"

namespace lambcalc {
namespace ast {

using AlphaRenamePipeline =
    WorklistVisitor<DefaultVisitor, WorklistTask<Exp>, std::stack>;
struct AlphaRenameVisitor : AlphaRenamePipeline {
  using AlphaRenamePipeline::operator();
};

void rename(ast::Exp &exp) {
  // TODO: rename the expression
}

} // namespace ast
} // namespace lambcalc
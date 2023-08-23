#include "CommonUtils.h"

#include "clang/AST/ParentMapContext.h"
#include "clang/AST/StmtOpenMP.h"

using namespace clang;

bool isPtrOrRefToConst(QualType Type) {
  if (!Type->isAnyPointerType() && !Type->isReferenceType())
    return false;

  while (Type->isAnyPointerType() || Type->isReferenceType()) {
    Type = Type->getPointeeType();
  }
  return Type.isConstQualified();
}

/* Searches the leftmost descendants for to find DeclRefExpr and returns it.
 * Returns nullptr if not found.
 */
const DeclRefExpr *getLeftmostDecl(const Stmt *S) {
  while (S) {
    S = *(S->child_begin());
    const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(S);
    if (DRE)
      return DRE;
  }
  return nullptr;
}

/* Returns a bool indicating if the ValueDecl was used in the given Stmt.
 */
bool usedInStmt(const Stmt *S, const ValueDecl *VD) {
  const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(S);
  if (DRE) {
    if (DRE->getDecl() == VD)
      return true;
    return false;
  }

  for (const Stmt *Child : S->children()) {
    bool usedInChild = usedInStmt(Child, VD);
    if (usedInChild)
      return true;
  }

  return false;
}

bool isaTargetKernel(const Stmt *S) {
  return isa<OMPTargetDirective>(S)
      || isa<OMPTargetParallelDirective>(S)
      || isa<OMPTargetParallelForDirective>(S)
      || isa<OMPTargetParallelForSimdDirective>(S)
      || isa<OMPTargetParallelGenericLoopDirective>(S)
      || isa<OMPTargetSimdDirective>(S)
      || isa<OMPTargetTeamsDirective>(S)
      || isa<OMPTargetTeamsDistributeDirective>(S)
      || isa<OMPTargetTeamsDistributeParallelForDirective>(S)
      || isa<OMPTargetTeamsDistributeParallelForSimdDirective>(S)
      || isa<OMPTargetTeamsDistributeSimdDirective>(S)
      || isa<OMPTargetTeamsGenericLoopDirective>(S)
      || isa<OMPTargetParallelGenericLoopDirective>(S);
}

#include "CommonUtils.h"

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
  * Returns NULL pointer if not found.
  */
DeclRefExpr *getLeftmostDecl(Stmt *S) {
  while (S) {
    S = *(S->child_begin());
    DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(S);
    if (DRE)
      return DRE;
  }
  return NULL;
}

/* Returns a bool indicating if the ValueDecl was used in the given Stmt.
  */
bool usedInStmt(Stmt *S, ValueDecl *VD) {
  DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(S);
  if (DRE) {
    if (DRE->getDecl() == VD)
      return true;
    return false;
  }

  for (Stmt *Child : S->children()) {
    bool usedInChild = usedInStmt(Child, VD);
    if (usedInChild)
      return true;
  }

  return false;
}

bool isaTargetKernel(Stmt *S) {
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
      || isa<OMPTargetTeamsGenericLoopDirective>(S);
}

/*
void emittingDiagnostic()
{
  clang::SourceLocation Loc = FD->getBeginLoc();
  // Create a warning message.
  clang::DiagnosticsEngine &DiagEngine = Context->getDiagnostics();
  const unsigned DiagID = DiagEngine.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                                      "I findz a badness");
  DiagnosticBuilder DiagBuilder = DiagEngine.Report(Loc, DiagID);
  // Optionally, you can set the SourceRange to highlight the relevant code in the warning message.
  DiagBuilder << SourceRange(Loc, FD->getEndLoc());
  return;
}
*/


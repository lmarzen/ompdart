#ifndef ROSEASTVISITOR_H
#define ROSEASTVISITOR_H

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"

#include "DataTracker.h"

using namespace clang;

class OmpDartASTVisitor : public RecursiveASTVisitor<OmpDartASTVisitor> {
private:
  ASTContext *Context;
  SourceManager *SM;

  // each DataTracker keeps track of data access within the scope of a single
  // function
  std::vector<DataTracker *> FunctionTrackers;
  std::vector<Kernel *> Kernels;

  DataTracker *LastFunction;
  Kernel *LastKernel;
  Stmt *LastStmt;

  bool inLastTargetRegion(SourceLocation Loc);
  bool inLastFunction(SourceLocation Loc);

public:
  explicit OmpDartASTVisitor(CompilerInstance *CI);

  std::vector<DataTracker *> &getFunctionTrackers();
  std::vector<Kernel *> &getTargetRegions(); 

  virtual bool VisitStmt(Stmt *S);
  virtual bool VisitFunctionDecl(FunctionDecl *FD);
  virtual bool VisitVarDecl(VarDecl *VD);
  virtual bool VisitCallExpr(CallExpr *CE);
  virtual bool VisitBinaryOperator(BinaryOperator *BO);
  virtual bool VisitUnaryOperator(UnaryOperator *UO);
  virtual bool VisitDeclRefExpr(DeclRefExpr *DRE);
  virtual bool VisitArraySubscriptExpr(ArraySubscriptExpr *ASE);
  virtual bool VisitDoStmt(DoStmt *DS);
  virtual bool VisitForStmt(ForStmt *FS);
  virtual bool VisitWhileStmt(WhileStmt *WS);
  virtual bool VisitIfStmt(IfStmt *IS);
  virtual bool VisitSwitchStmt(SwitchStmt *SS);
  virtual bool VisitOMPExecutableDirective(OMPExecutableDirective *S);

}; // end class OmpDartASTVisitor

#endif
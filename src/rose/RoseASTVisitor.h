#ifndef ROSEASTVISITOR_H
#define ROSEASTVISITOR_H

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"

#include "DataTracker.h"

using namespace clang;

class RoseASTVisitor : public RecursiveASTVisitor<RoseASTVisitor> {
private:
  ASTContext *Context;
  SourceManager *SM;

  // each DataTracker keeps track of data access within the scope of a single
  // function
  std::vector<DataTracker *> FunctionTrackers;
  std::vector<TargetRegion *> TargetRegions;

  DataTracker *LastFunction;
  TargetRegion *LastTargetRegion;
  Stmt *LastStmt;

  bool inLastTargetRegion(SourceLocation Loc);
  bool inLastFunction(SourceLocation Loc);

public:
  explicit RoseASTVisitor(CompilerInstance *CI);

  std::vector<DataTracker *> &getFunctionTrackers();
  std::vector<TargetRegion *> &getTargetRegions(); 

  virtual bool VisitStmt(Stmt *S);
  virtual bool VisitFunctionDecl(FunctionDecl *FD);
  virtual bool VisitVarDecl(VarDecl *VD);
  virtual bool VisitCallExpr(CallExpr *CE);
  virtual bool VisitBinaryOperator(BinaryOperator *BO);
  virtual bool VisitUnaryOperator(UnaryOperator *UO);
  virtual bool VisitDeclRefExpr(DeclRefExpr *DRE);
  virtual bool VisitDoStmt(DoStmt *DS);
  virtual bool VisitForStmt(ForStmt *FS);
  virtual bool VisitWhileStmt(WhileStmt *WS);
  virtual bool VisitOMPExecutableDirective(OMPExecutableDirective *S);

}; // end class RoseASTVisitor

#endif
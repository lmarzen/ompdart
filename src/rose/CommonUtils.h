#ifndef COMMONUTILS_H
#define COMMONUTILS_H

#include "clang/AST/Type.h"
#include "clang/AST/Expr.h"

using namespace clang;

bool isPtrOrRefToConst(QualType Type);
bool isMemAlloc(const FunctionDecl *Callee);
bool isMemDealloc(const FunctionDecl *Callee);
const DeclRefExpr *getLeftmostDecl(const Stmt *S);
bool usedInStmt(const Stmt *S, const ValueDecl *VD);
bool isaTargetKernel(const Stmt *S);
const ArraySubscriptExpr *fetchArraySubscript(ASTContext *Context,
                                              const DeclRefExpr *DRE);

#endif
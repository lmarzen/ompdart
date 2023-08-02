#ifndef COMMONUTILS_H
#define COMMONUTILS_H

#include "clang/AST/Type.h"
#include "clang/AST/Expr.h"

using namespace clang;

bool isPtrOrRefToConst(QualType Type);
DeclRefExpr *getLeftmostDecl(Stmt *S);
bool usedInStmt(Stmt *S, ValueDecl *VD);
bool isaTargetKernel(Stmt *S);

#endif
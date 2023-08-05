#ifndef CLAUSEINFO_H
#define CLAUSEINFO_H

#include "clang/AST/StmtOpenMP.h"

using namespace clang;

struct ClauseInfo {
  OMPExecutableDirective *Directive;
  ValueDecl *VD;

  ClauseInfo(OMPExecutableDirective *Directive, ValueDecl *VD)
      : Directive(Directive), VD(VD) {}
};

#endif
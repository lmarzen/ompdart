#ifndef CLAUSEINFO_H
#define CLAUSEINFO_H

#include "clang/AST/StmtOpenMP.h"

using namespace clang;

struct ClauseInfo {
  const OMPExecutableDirective *Directive;
  const ValueDecl *VD;

  ClauseInfo(const OMPExecutableDirective *Directive, const ValueDecl *VD)
      : Directive(Directive), VD(VD) {}
};

#endif
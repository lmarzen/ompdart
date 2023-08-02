#ifndef DIRECTIVEREWRITER_H
#define DIRECTIVEREWRITER_H

#include "clang/Rewrite/Core/Rewriter.h"

#include "TargetDataScope.h"

using namespace clang;

void rewriteTargetDataScope(Rewriter &R, ASTContext &Context, const TargetDataScope *Data);

#endif
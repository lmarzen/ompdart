#ifndef DIRECTIVEREWRITER_H
#define DIRECTIVEREWRITER_H

#include "clang/Rewrite/Core/Rewriter.h"

#include "TargetDataRegion.h"

using namespace clang;

void rewriteTargetDataRegion(Rewriter &R, ASTContext &Context, const TargetDataRegion *Data);

#endif
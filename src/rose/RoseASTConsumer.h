#ifndef ROSEASTCONSUMER_H
#define ROSEASTCONSUMER_H

#include "clang/Rewrite/Core/Rewriter.h"

#include "RoseASTVisitor.h"

using namespace clang;

class RoseASTConsumer : public ASTConsumer {
  ASTContext *Context;
  SourceManager *SM;
  RoseASTVisitor *Visitor;
  Rewriter TheRewriter;

  std::vector<DataTracker *> &FunctionTrackers;
  std::vector<Kernel *> &Kernels;

public:
  explicit RoseASTConsumer(CompilerInstance *CI);
  
  virtual void HandleTranslationUnit(ASTContext &Context);

}; // end class RoseASTConsumer

#endif

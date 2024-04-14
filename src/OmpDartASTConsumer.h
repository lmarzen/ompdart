#ifndef ROSEASTCONSUMER_H
#define ROSEASTCONSUMER_H

#include "clang/Rewrite/Core/Rewriter.h"

#include "OmpDartASTVisitor.h"

using namespace clang;

class OmpDartASTConsumer : public ASTConsumer {
  ASTContext *Context;
  SourceManager *SM;
  OmpDartASTVisitor *Visitor;
  Rewriter TheRewriter;
  std::string OutFilePath;
  bool Aggressive;

  std::vector<DataTracker *> &FunctionTrackers;
  std::vector<Kernel *> &Kernels;

public:
  explicit OmpDartASTConsumer(CompilerInstance *CI,
                              const std::string *OutFilePath, bool Aggressive);

  virtual void HandleTranslationUnit(ASTContext &Context);

}; // end class OmpDartASTConsumer

#endif

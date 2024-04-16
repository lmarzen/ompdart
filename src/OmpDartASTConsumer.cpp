#include "OmpDartASTConsumer.h"

#include "AnalysisUtils.h"
#include "DirectiveRewriter.h"
#include <string>

using namespace clang;

OmpDartASTConsumer::OmpDartASTConsumer(CompilerInstance *CI,
                                       const std::string *OutFilePath,
                                       bool Aggressive)
    : Context(&(CI->getASTContext())), SM(&(Context->getSourceManager())),
      Visitor(new OmpDartASTVisitor(CI)),
      FunctionTrackers(Visitor->getFunctionTrackers()),
      Kernels(Visitor->getTargetRegions()) {
  TheRewriter.setSourceMgr(*SM, Context->getLangOpts());
  this->OutFilePath = *OutFilePath;
  this->Aggressive = Aggressive;
}

void OmpDartASTConsumer::HandleTranslationUnit(ASTContext &Context) {
  Visitor->TraverseDecl(Context.getTranslationUnitDecl());

  performInterproceduralAnalysis(FunctionTrackers);

  for (DataTracker *DT : FunctionTrackers) {
    DT->classifyOffloadedOps();
  }

  if (Aggressive)
    performAggressiveCrossFunctionOffloading(FunctionTrackers);

#if DEBUG_LEVEL >= 1
  llvm::outs() << "\n=========================================================="
                  "======================\n";
#endif
  for (DataTracker *DT : FunctionTrackers) {
    // DT->classifyOffloadedOps();
#if DEBUG_LEVEL >= 1
    DT->printAccessLog();
#endif
    // computes data mappings for the scope of single target regions
    DT->naiveAnalyze();
    // computes data mappings
    DT->analyze();
#if DEBUG_LEVEL >= 1
    llvm::outs() << "globals\n";
    for (auto Global : DT->getGlobals()) {
      llvm::outs() << "  " << Global->getNameAsString() << "  "
                   << Global->getID() << "\n";
    }
    llvm::outs() << "locals\n";
    for (auto Local : DT->getLocals()) {
      llvm::outs() << "  " << Local->getNameAsString() << "  " << Local->getID()
                   << "\n";
    }
#endif
  }

#if DEBUG_LEVEL >= 1
  llvm::outs() << "Number of Target Data Regions: " << Kernels.size() << "\n";

  // Print naive kernel information
  for (int I = 0; I < Kernels.size(); ++I) {
    llvm::outs() << "\nTargetRegion #" << I;
    Kernels[I]->print(llvm::outs(), *SM);
  }

  int I = 0;
#endif
  for (DataTracker *DT : FunctionTrackers) {
    const TargetDataRegion *Scope = DT->getTargetDataScope();
    if (!Scope)
      continue;
#if DEBUG_LEVEL >= 1
    llvm::outs() << "\nTargetScope #" << I++;
    Scope->print(llvm::outs(), *SM);
#endif
    rewriteTargetDataRegion(TheRewriter, Context, Scope);
  }

#if DEBUG_LEVEL >= 1
  for (DataTracker *DT : FunctionTrackers) {
    const FunctionDecl *funcDecl = DT->getDecl();
    Stmt *funcBody = funcDecl->getBody();
    static std::unique_ptr<CFG> sourceCFG =
        CFG::buildCFG(funcDecl, funcBody, &Context, clang::CFG::BuildOptions());
    auto langOpt = Context.getLangOpts();
    sourceCFG->dump(langOpt, true);
  }
#endif

  FileID FID = SM->getMainFileID();
  if (OutFilePath.empty()) {
    std::string ParsedFilename =
        SM->getFilename(SM->getLocForStartOfFile(FID)).str();
    char *CParsedFilename = strdup(ParsedFilename.c_str());
    char *Basename = basename(CParsedFilename);

    OutFilePath = "/tmp/" + std::string(Basename);
    free(CParsedFilename);
  }
  llvm::outs() << "Modified File at " << OutFilePath << "\n";
  std::error_code ErrorCode;
  llvm::raw_fd_ostream OutFile(OutFilePath, ErrorCode, llvm::sys::fs::OF_None);
  if (!ErrorCode) {
    // print to terminal
    // TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
    // write to OutFile
    TheRewriter.getEditBuffer(FID).write(OutFile);
  } else {
    llvm::outs() << "Could not create file\n";
  }
  OutFile.close();
}

#include "RoseASTConsumer.h"

#include "AnalysisUtils.h"
#include "DirectiveRewriter.h"

using namespace clang;

RoseASTConsumer::RoseASTConsumer(CompilerInstance *CI)
    : Context(&(CI->getASTContext())),
      SM(&(Context->getSourceManager())),
      Visitor(new RoseASTVisitor(CI)),
      FunctionTrackers(Visitor->getFunctionTrackers()),
      Kernels(Visitor->getTargetRegions()) {
  TheRewriter.setSourceMgr(*SM, Context->getLangOpts());
}

void RoseASTConsumer::HandleTranslationUnit(ASTContext &Context) {
  Visitor->TraverseDecl(Context.getTranslationUnitDecl());

  performInterproceduralAnalysis(FunctionTrackers);

  llvm::outs() << "\n================================================================================n";
  for (DataTracker *DT : FunctionTrackers) {
    DT->classifyOffloadedOps();
    DT->printAccessLog();
    // computes data mappings for the scope of single target regions
    DT->naiveAnalyze();
    // computes data mappings
    DT->analyze();
    llvm::outs() << "globals\n";
    for (auto Global : DT->getGlobals()) {
      llvm::outs() << "  " << Global->getNameAsString() 
                   << "  " << Global->getID() << "\n";
    }
    llvm::outs() << "locals\n";
    for (auto Local : DT->getLocals()) {
      llvm::outs() << "  " << Local->getNameAsString()
                   << "  " << Local->getID() << "\n";
    }
  }

  llvm::outs() << "Number of Target Data Regions: " << Kernels.size() << "\n";

  // Print naive kernel information
  for (int I = 0; I < Kernels.size(); ++I) {
    llvm::outs() << "\nTargetRegion #" << I;
    Kernels[I]->print(llvm::outs(), *SM);
  }

  int I = 0;
  for (DataTracker *DT : FunctionTrackers) {
    const TargetDataRegion *Scope = DT->getTargetDataScope();
    if (!Scope)
      continue;
    llvm::outs() << "\nTargetScope #" << I++;
    Scope->print(llvm::outs(), *SM);
    rewriteTargetDataRegion(TheRewriter, Context, Scope);
  }

  
  for (DataTracker *DT : FunctionTrackers) {
    const FunctionDecl *funcDecl = DT->getDecl();
    Stmt *funcBody = funcDecl->getBody();
    static std::unique_ptr<CFG> sourceCFG = CFG::buildCFG(
          funcDecl, funcBody, &Context, clang::CFG::BuildOptions());
      auto langOpt = Context.getLangOpts();
      sourceCFG->dump(langOpt, true);
  }

  FileID FID = SM->getMainFileID();
  std::string ParsedFilename = SM->getFilename(SM->getLocForStartOfFile(FID)).str();
  char *CParsedFilename = strdup(ParsedFilename.c_str());
  char* Basename = basename(CParsedFilename);

  std::string Filename = "/tmp/" + std::string(Basename);
  llvm::outs() << "Modified File at " << Filename << "\n";
  std::error_code ErrorCode;
  llvm::raw_fd_ostream OutFile(Filename, ErrorCode, llvm::sys::fs::OF_None);
  if (!ErrorCode) {
    // print to terminal
    // TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
    // write to OutFile
    TheRewriter.getEditBuffer(FID).write(OutFile);
  } else {
    llvm::outs() << "Could not create file\n";
  }
  OutFile.close();
  free(CParsedFilename);
}

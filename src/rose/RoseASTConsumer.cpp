#include "RoseASTConsumer.h"

#include "DirectiveRewriter.h"

using namespace clang;

RoseASTConsumer::RoseASTConsumer(CompilerInstance *CI)
    : Context(&(CI->getASTContext())),
      SM(&(Context->getSourceManager())),
      Visitor(new RoseASTVisitor(CI)),
      FunctionTrackers(Visitor->getFunctionTrackers()),
      TargetRegions(Visitor->getTargetRegions()) {
  TheRewriter.setSourceMgr(*SM, Context->getLangOpts());
}

void RoseASTConsumer::HandleTranslationUnit(ASTContext &Context) {
  Visitor->TraverseDecl(Context.getTranslationUnitDecl());

  // Using the information we have collected about read and writes we can
  // update calls to functions with the details about how a pointer was used
  // after it was passed. We may need to do this multiple times as we may need
  // to resolve more information about one function before we can update
  // another. So we loop until the number of unknown parameters converges.
  // TODO: This iterative approach is simplier than a recursive algorithm, but
  //       inherently cannot resolve recursions.
  int numUpdates;
  do {
    numUpdates = 0;
    for (DataTracker *DT : FunctionTrackers) {
      for (DataTracker *TmpDT : FunctionTrackers) {
        numUpdates += TmpDT->updateTouchedByCallee(
                        DT->getDecl(),    DT->getParamAccessModes(),
                        DT->getGlobals(), DT->getGlobalAccessModes());
      }
    }
    llvm::outs() << numUpdates << "\n";
  } while (numUpdates > 0);

  llvm::outs() << "\n================================================================================n";
  for (DataTracker *DT : FunctionTrackers) {
    DT->printAccessLog();
    // computes data mappings for the scope of single target regions
    DT->naiveAnalyze();
    // computes data mappings
    DT->analyze();
    llvm::outs() << "globals\n";
    for (auto Global : DT->getGlobals()) {
      llvm::outs() << "  " << Global->getNameAsString() << "\n";
    }
    llvm::outs() << "locals\n";
    for (auto Local : DT->getLocals()) {
      llvm::outs() << "  " << Local->getNameAsString() << "\n";
          TheRewriter.InsertTextBefore(Local->getBeginLoc(),"/* test */");
    }
  }

  llvm::outs() << "Number of Target Data Regions: " << TargetRegions.size() << "\n";

  // Print naive kernel information
  for (int I = 0; I < TargetRegions.size(); ++I) {
    llvm::outs() << "\nTargetRegion #" << I;
    TargetRegions[I]->print(llvm::outs(), *SM);
  }

  int I = 0;
  for (DataTracker *DT : FunctionTrackers) {
    const TargetDataScope *Scope = DT->getTargetDataScope();
    if (!Scope)
      continue;
    llvm::outs() << "\nTargetScope #" << I++;
    Scope->print(llvm::outs(), *SM);
    rewriteTargetDataScope(TheRewriter, Context, Scope);
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

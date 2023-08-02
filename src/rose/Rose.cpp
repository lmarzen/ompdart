#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"

// #include "clang/AST/Type.h"
#include "llvm/Support/raw_ostream.h"
// #include "clang/AST/ASTContext.h"

// #include "clang/Driver/Options.h"
// #include "clang/AST/ASTContext.h"
// #include "clang/AST/Mangle.h"
// #include "clang/Frontend/ASTConsumers.h"
// #include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "clang/Lex/Lexer.h"

#include "DataTracker.h"
#include "DataTracker.h"
#include "TargetRegion.h"
#include "CommonUtils.h"
#include "DirectiveRewriter.h"
#include <sys/time.h>
#include <libgen.h>

using namespace clang;

namespace {

clang::Rewriter TheRewriter;

// each DataTracker keeps track of data access within the scope of a single
// function
std::vector<DataTracker *> FunctionTrackers;
std::vector<TargetRegion *> TargetRegions;

class TargetRegionInfoVisitor : public RecursiveASTVisitor<TargetRegionInfoVisitor> {
private:
  ASTContext *Context;
  SourceManager *SM;

  DataTracker *LastFunction;
  TargetRegion *LastTargetRegion;
  Stmt *LastStmt;

  bool inLastTargetRegion(SourceLocation Loc) {
    if (!LastTargetRegion)
      return false;
    return LastTargetRegion->contains(Loc);
  }

  bool inLastFunction(SourceLocation Loc) {
    if (!LastFunction)
      return false;
    return LastFunction->contains(Loc);
  }


public:
  explicit TargetRegionInfoVisitor(CompilerInstance *CI)
      : Context(&(CI->getASTContext())),
        SM(&(Context->getSourceManager())) {
    TheRewriter.setSourceMgr(*SM, Context->getLangOpts());
    LastTargetRegion = NULL;
    LastFunction = NULL;
  }

  virtual bool VisitFunctionDecl(FunctionDecl *FD) {
    if (!FD->getBeginLoc().isValid()
     || !SM->isInMainFile(FD->getLocation()))
      return true;
    if (!FD->doesThisDeclarationHaveABody())
      return true;

    DataTracker *Tracker = new DataTracker(FD, Context);
    FunctionTrackers.push_back(Tracker);
    LastFunction = Tracker;
    return true;
  }

  virtual bool VisitVarDecl(VarDecl *VD) {
    if (!VD->getLocation().isValid()
     || !SM->isInMainFile(VD->getLocation()))
      return true;
    if (inLastTargetRegion(VD->getLocation())) {
      LastTargetRegion->recordPrivate(VD);
      return true;
    }

    if (inLastFunction(VD->getLocation())) {
      LastFunction->recordLocal(VD);
      uint8_t Flags = VD->hasInit() ? A_WRONLY : A_NOP;
      LastFunction->recordAccess(VD, VD->getLocation(), LastStmt, Flags, true);
      return true;
    }

    return true;
  }

  virtual bool VisitCallExpr(CallExpr *CE) {
    if (!CE->getBeginLoc().isValid()
     || !SM->isInMainFile(CE->getBeginLoc()))
      return true;
    if (!inLastFunction(CE->getBeginLoc()))
      return true;
    FunctionDecl *Callee = CE->getDirectCallee();
    if (!Callee)
      return true;
    
    LastFunction->recordCallExpr(CE);
    Expr **Args = CE->getArgs();

    for (int I = 0; I < CE->getNumArgs(); ++I) {
      DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Args[I]->IgnoreImpCasts());
      QualType ParamType = Callee->getParamDecl(I)->getType();
      if (!DRE) {
        // is a literal
        continue;
      }
      ValueDecl *VD = DRE->getDecl();
      if ( (ParamType->isPointerType() || ParamType->isReferenceType() )
        && !isPtrOrRefToConst(ParamType)) {
        // passed by pointer/reference (to non-const)
        LastFunction->recordAccess(VD, DRE->getLocation(), CE, A_UNKNOWN, true);
      } else {
        // passed by pointer/reference (to const) OR passed by value
        LastFunction->recordAccess(VD, DRE->getLocation(), CE, A_RDONLY, true);
      }
    }

    return true;
  }

  virtual bool VisitBinaryOperator(BinaryOperator *BO) {
    if (!BO->getBeginLoc().isValid()
     || !SM->isInMainFile(BO->getBeginLoc()))
      return true;
    if (!BO->isAssignmentOp())
      return true;
    if (!inLastFunction(BO->getBeginLoc()))
      return true;

    DeclRefExpr *DRE = getLeftmostDecl(BO);
    ValueDecl *VD = DRE->getDecl();

    // Check to see if this value is read from the right hand side.
    if (BO->isCompoundAssignmentOp() || usedInStmt(BO->getRHS(), VD)) {
      // If value is read from the right hand side, then technically this is a
      // read, but chronologically it was read first. So mark as ReadWrite so
      // that we don't mistake this ValueDecl for being Writen to first.
      LastFunction->recordAccess(VD, DRE->getLocation(), BO, A_RDWR, true);
    } else {
      LastFunction->recordAccess(VD, DRE->getLocation(), BO, A_WRONLY, true);
    }

    return true;
  }

  virtual bool VisitUnaryOperator(UnaryOperator *UO) {
    if (!UO->getBeginLoc().isValid()
     || !SM->isInMainFile(UO->getBeginLoc()))
      return true;
    if (!(UO->isPostfix() || UO->isPrefix()))
      return true;
    if (!inLastFunction(UO->getBeginLoc()))
      return true;

    DeclRefExpr *DRE = getLeftmostDecl(UO);
    ValueDecl *VD = DRE->getDecl();
    LastFunction->recordAccess(VD, DRE->getLocation(), UO, A_RDWR, true);
    return true;
  }

  virtual bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    if (!DRE->getBeginLoc().isValid()
     || !SM->isInMainFile(DRE->getBeginLoc()))
      return true;
    if (!inLastFunction(DRE->getBeginLoc()))
      return true;
    ValueDecl *VD = DRE->getDecl();
    if (!VD)
      return true;
    if (dyn_cast<clang::FunctionDecl>(VD))
      return true;

    LastFunction->recordAccess(VD, DRE->getLocation(), DRE, A_RDONLY, false);
    return true;
  }

  virtual bool VisitDoStmt(DoStmt *DS) {
    if (!DS->getBeginLoc().isValid()
     || !SM->isInMainFile(DS->getBeginLoc()))
      return true;
    if (!inLastFunction(DS->getBeginLoc()))
      return true;

    LastFunction->recordLoop(DS);
    return true;
  }

  virtual bool VisitForStmt(ForStmt *FS) {
    if (!FS->getBeginLoc().isValid()
     || !SM->isInMainFile(FS->getBeginLoc()))
      return true;
    if (!inLastFunction(FS->getBeginLoc()))
      return true;

    LastFunction->recordLoop(FS);
    return true;
  }

  virtual bool VisitWhileStmt(WhileStmt *WS) {
    if (!WS->getBeginLoc().isValid()
     || !SM->isInMainFile(WS->getBeginLoc()))
      return true;
    if (!inLastFunction(WS->getBeginLoc()))
      return true;

    LastFunction->recordLoop(WS);
    return true;
  }

  virtual bool VisitOMPExecutableDirective(OMPExecutableDirective *S) {
    // Ignore if the statement is in System Header files
    if (!S->getBeginLoc().isValid() ||
        !SM->isInMainFile(S->getBeginLoc()))
      return true;
    if (!isaTargetKernel(S))
      return true;

    LastTargetRegion = new TargetRegion(S, LastFunction->getDecl());
    FunctionTrackers.back()->recordTargetRegion(LastTargetRegion);
    TargetRegions.push_back(LastTargetRegion);
    return true;
  }

}; // end class TargetRegionInfoVisitor

class TargetDataAnalysisConsumer : public ASTConsumer {
  TargetRegionInfoVisitor *visitor;

public:
  explicit TargetDataAnalysisConsumer(CompilerInstance *CI)
      : visitor(new TargetRegionInfoVisitor(CI)) {}
  
  virtual void HandleTranslationUnit(ASTContext &Context) {
    visitor->TraverseDecl(Context.getTranslationUnitDecl());

    SourceManager &SM = Context.getSourceManager();

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
      TargetRegions[I]->print(llvm::outs(), SM);
    }

    int I = 0;
    for (DataTracker *DT : FunctionTrackers) {
      const TargetDataScope *Scope = DT->getTargetDataScope();
      if (!Scope)
        continue;
      llvm::outs() << "\nTargetScope #" << I++;
      Scope->print(llvm::outs(), SM);
      rewriteTargetDataScope(TheRewriter, Context, Scope);
    }

    FileID FID = SM.getMainFileID();
    std::string ParsedFilename = SM.getFilename(SM.getLocForStartOfFile(FID)).str();
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

}; // end class TargetDataAnalysisConsumer

class TargetDataAnalysisAction : public PluginASTAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    return std::make_unique<TargetDataAnalysisConsumer>(&CI);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
  void PrintHelp(llvm::raw_ostream& ros) {
    ros << "TODO:: Help goes here\n";
    return;
  }

}; // end class TargetDataAnalysisAction

} // end anonymous namespace

static FrontendPluginRegistry::Add<TargetDataAnalysisAction>
X("-rose", "target data analysis");

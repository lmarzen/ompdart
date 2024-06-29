#include "DataTracker.h"

#include "clang/AST/ParentMapContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"

#include "CommonUtils.h"

using namespace clang;

struct LoopDependency {
  bool DataValidOnHost;
  bool DataValidOnDevice;
  bool MapTo;
  AccessInfo *FirstHostAccess;
};

struct CondDependency {
  AccessInfo Conditional;
};

DataTracker::DataTracker(FunctionDecl *FD, ASTContext *Context) {
  this->FD = FD;
  this->Context = Context;
  for (ValueDecl *Param : FD->parameters()) {
    this->Locals.insert(Param);
  }
  this->LastKernel = nullptr;
  this->TargetScope = nullptr;

  this->LastArrayBasePointer = nullptr;
  this->LastArraySubscript = nullptr;
};

const FunctionDecl *DataTracker::getDecl() const { return FD; }

bool DataTracker::contains(SourceLocation Loc) const {
  SourceManager &SM = Context->getSourceManager();
  Stmt *Body = FD->getBody();
  SourceLocation BodyBeginLoc = Body->getBeginLoc();
  SourceLocation BodyEndLoc = Body->getEndLoc();
  return !SM.isBeforeInTranslationUnit(Loc, BodyBeginLoc) &&
         SM.isBeforeInTranslationUnit(Loc, BodyEndLoc);
}

int DataTracker::insertAccessLogEntry(const AccessInfo &NewEntry) {
  if (AccessLog.empty()) {
    AccessLog.push_back(NewEntry);
    return 1;
  }

  // Insert into AccessLog. Maintain order of increasing SourceLocation.
  // New accesses will usually be somewhere within the last few SourceLocations
  // so iterating backwards is quick.
  std::vector<AccessInfo>::iterator It = AccessLog.end();
  SourceManager &SM = Context->getSourceManager();
  do {
    --It;
  } while (SM.isBeforeInTranslationUnit(NewEntry.Loc, It->Loc));

  // insert at correct position in AccessLog
  ++It;
  AccessLog.insert(It, NewEntry);
  return 1;
}

int DataTracker::recordAccess(const ValueDecl *VD, SourceLocation Loc,
                              const Stmt *S, uint8_t Flags, bool overwrite) {
  SourceManager &SM = Context->getSourceManager();
  if (LastKernel) {
    // Don't record private data.
    if (LastKernel->isPrivate(VD))
      return 0;

    // Clang duplicates references when they are declared in function parameters
    // and then used in openmp target regions. These can be filtered out easily
    // as the duplicate will have the same location as the directive statement.
    // Additionally, clang may create temporary variables when using openmp
    // target directives. These can be filtered out by ignoring all variables
    // that have names that begin with a period(.). Any variables used in any
    // nested directive must be ignored here.
    if (VD->getNameAsString()[0] == '.')
      return 0;

    if (!SM.isBeforeInTranslationUnit(
            VD->getBeginLoc(), LastKernel->getDirective()->getBeginLoc()) &&
        SM.isBeforeInTranslationUnit(VD->getBeginLoc(),
                                     LastKernel->getDirective()->getEndLoc()))
      return 0;

    if (!SM.isBeforeInTranslationUnit(
            Loc, LastKernel->getDirective()->getBeginLoc()) &&
        SM.isBeforeInTranslationUnit(Loc,
                                     LastKernel->getDirective()->getEndLoc()))
      return 0;

    if (LastKernel->NestedDirectives.size() > 0) {
      auto *NestedDir = LastKernel->NestedDirectives.back();
      if (!SM.isBeforeInTranslationUnit(Loc, NestedDir->getBeginLoc()) &&
          SM.isBeforeInTranslationUnit(Loc, NestedDir->getEndLoc()))
        return 0;
    }

    // if (LastKernel->contains(Loc))
    //   Flags |= A_OFFLD;
  }

  // check for existing log entry
  for (int I = AccessLog.size() - 1; I >= 0; --I) {
    if (AccessLog[I].VD == VD && AccessLog[I].Loc == Loc) {
      if (!overwrite || AccessLog[I].Flags == Flags)
        return 0;
      AccessLog[I].Flags = Flags;
      return 1;
    }
  }

  if (!Locals.contains(VD))
    recordGlobal(VD);

  AccessInfo NewEntry = {};
  NewEntry.VD = VD;
  NewEntry.S = S;
  NewEntry.Loc = Loc;
  NewEntry.Flags = Flags;

  if (VD && VD == LastArrayBasePointer) {
    NewEntry.ArraySubscript = LastArraySubscript;
    LastArrayBasePointer = nullptr;
    LastArraySubscript = nullptr;
  }

  return insertAccessLogEntry(NewEntry);
}

const std::vector<AccessInfo> &DataTracker::getAccessLog() { return AccessLog; }

/* Update reads/writes that may have happened on by the Callee parameters passed
 * by pointer.
 */
int DataTracker::updateParamsTouchedByCallee(
    const FunctionDecl *Callee, const std::vector<const CallExpr *> &Calls,
    const std::vector<uint8_t> &ParamModes) {
  int numUpdates = 0;
  if (Callee->getNumParams() != ParamModes.size()) {
    llvm::outs() << "\nwarning: unable to update parameters for function "
                 << FD->getNameAsString()
                 << " with parameter access modes of callee "
                 << Callee->getNameAsString()
                 << " Unequal number of parameters.";
    return numUpdates;
  }
  std::vector<ParmVarDecl *> Params = Callee->parameters();
  if (Params.size() == 0)
    return numUpdates;

  for (const CallExpr *CE : Calls) {
    const Expr *const *Args = CE->getArgs();

    for (int I = 0; I < Callee->getNumParams(); ++I) {
      QualType ParamType = Callee->getParamDecl(I)->getType();
      if (!ParamType->isPointerType() && !ParamType->isReferenceType())
        continue;
      const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Args[I]->IgnoreCasts());
      if (!DRE) {
        // is a literal or expression
        continue;
      }
      const ValueDecl *VD = DRE->getDecl();
      numUpdates +=
          recordAccess(VD, DRE->getLocation(), nullptr, ParamModes[I], true);
    }
  }
  return numUpdates;
}

/* Update reads/writes that may have occurred by the Callee on global variables.
 * We will need to insert these into our own AccessLog.
 */
int DataTracker::updateGlobalsTouchedByCallee(
    const FunctionDecl *Callee, const std::vector<const CallExpr *> &Calls,
    const boost::container::flat_set<const ValueDecl *> &GlobalsAccessed,
    const std::vector<uint8_t> &GlobalModes) {
  int numUpdates = 0;

  if (GlobalsAccessed.size() != GlobalModes.size()) {
    llvm::outs() << "\nwarning: unable to update globals for function "
                 << FD->getNameAsString()
                 << " with global access modes of callee "
                 << Callee->getNameAsString()
                 << " Unequal number of parameters.";
    return numUpdates;
  }

  int I = 0;
  for (const ValueDecl *Global : GlobalsAccessed) {
    for (const CallExpr *CE : Calls) {
      numUpdates += recordAccess(Global, CE->getBeginLoc(), nullptr,
                                 GlobalModes[I], true);
    }
    recordGlobal(Global);
    ++I;
  }

  return numUpdates;
}

int DataTracker::updateTouchedByCallee(
    const FunctionDecl *Callee, const std::vector<uint8_t> &ParamModes,
    const boost::container::flat_set<const ValueDecl *> &GlobalsAccessed,
    const std::vector<uint8_t> &GlobalModes) {
  int numUpdates = 0;
  // Start by finding all the calls to the callee.
  std::vector<const CallExpr *> Calls;
  for (const CallExpr *CE : CallExprs) {
    if (CE->getDirectCallee()->getDefinition() == Callee) {
      Calls.push_back(CE);
#if DEBUG_LEVEL >= 1
      llvm::outs() << "--> " << this->getDecl()->getNameAsString() << " calls "
                   << Callee->getNameAsString() << "\n";
#endif
    }
  }

  if (Calls.size() == 0)
    return numUpdates;

  numUpdates +=
      updateGlobalsTouchedByCallee(Callee, Calls, GlobalsAccessed, GlobalModes);
  numUpdates += updateParamsTouchedByCallee(Callee, Calls, ParamModes);
  return numUpdates;
}

void DataTracker::printAccessLog() const {
  SourceManager &SourceMgr = Context->getSourceManager();
  llvm::outs() << "\nAccess Log for function " << FD->getNameAsString() << "\n";

  size_t WidthVD = sizeof("ValueDecl");
  for (const AccessInfo &Entry : AccessLog) {
    if (Entry.VD)
      WidthVD = std::max(WidthVD, Entry.VD->getNameAsString().size() + 1);
  }
  size_t ExtraSpace = WidthVD - sizeof("ValueDecl");
  llvm::outs() << "ValueDecl ";
  llvm::outs() << std::string(ExtraSpace, ' ');
  llvm::outs() << "| Flags      | Exec   | Location\n";
  for (AccessInfo Entry : AccessLog) {
    if (!Entry.VD)
      continue;

    llvm::outs() << Entry.VD->getNameAsString();
    llvm::outs() << " " << Entry.VD->getID();
    ExtraSpace = WidthVD - Entry.VD->getNameAsString().size() + 1;

    if (Entry.ArraySubscript) {
      llvm::outs() << "  ";
      Entry.ArraySubscript->getBase()->printPretty(llvm::outs(), NULL,
                                                   Context->getLangOpts());
      llvm::outs() << "[";
      Entry.ArraySubscript->getIdx()->printPretty(llvm::outs(), NULL,
                                                  Context->getLangOpts());
      llvm::outs() << "]";
    }

    llvm::outs() << std::string(ExtraSpace + 1, ' ');
    switch (Entry.Flags & (A_RDWR | A_UNKNOWN)) {
    case A_UNKNOWN:
      llvm::outs() << "Unknown      ";
      break;
    case A_NOP:
      llvm::outs() << "NOP          ";
      break;
    case A_RDONLY:
      llvm::outs() << "Read         ";
      break;
    case A_WRONLY:
      llvm::outs() << "Write        ";
      break;
    case A_RDWR:
      llvm::outs() << "ReadWrite    ";
      break;
    default:
      llvm::outs() << "???          ";
      break;
    }
    if (Entry.Flags & A_OFFLD) {
      llvm::outs() << "TGTDEV   ";
    } else {
      llvm::outs() << "HOST     ";
    }
    llvm::outs() << Entry.Loc.printToString(SourceMgr) << "\n";
  }
  llvm::outs() << "\n";
  return;
}

int DataTracker::recordArrayAccess(const ValueDecl *BasePointer,
                                   const ArraySubscriptExpr *Subscript) {
  auto It = std::find_if(AccessLog.begin(), AccessLog.end(),
                         [BasePointer, Subscript](AccessInfo &A) {
                           return A.VD == BasePointer &&
                                  A.Loc == Subscript->getBeginLoc();
                         });

  if (It == AccessLog.end()) {
    // We have parsed the array subscript before determining the access type.
    // Save it so it can be attached when the access is record in the access
    // log.
    LastArrayBasePointer = BasePointer;
    LastArraySubscript = Subscript;
    return 1;
  }

  It->ArraySubscript = Subscript;
  LastArrayBasePointer = nullptr;
  LastArraySubscript = nullptr;
  return 1;
}

int DataTracker::recordTargetRegion(Kernel *K) {
  LastKernel = K;
  Kernels.push_back(K);

  AccessInfo NewEntry = {};
  NewEntry.VD = nullptr;
  NewEntry.S = K->getDirective();
  NewEntry.Loc = K->getBeginLoc();
  NewEntry.Flags = A_NOP;
  NewEntry.Barrier = ScopeBarrier::KernelBegin;
  insertAccessLogEntry(NewEntry);
  NewEntry.Loc = K->getEndLoc();
  NewEntry.Barrier = ScopeBarrier::KernelEnd;
  insertAccessLogEntry(NewEntry);
  return 1;
}

int DataTracker::recordCallExpr(const CallExpr *CE) {
  CallExprs.push_back(CE);
  return 1;
}

int DataTracker::recordLoop(const Stmt *S) {
  Loops.push_back(S);

  AccessInfo NewEntry = {};
  NewEntry.VD = nullptr;
  NewEntry.S = S;
  NewEntry.Loc = S->getBeginLoc();
  NewEntry.Flags = A_NOP;
  if (LastKernel != nullptr && LastKernel->contains(S->getBeginLoc()))
    NewEntry.Flags |= A_OFFLD;

  // Analyze loop bounds. This will be used for array bounds analysis.
  if (isa<ForStmt>(S)) {
    const ForStmt *FS = dyn_cast<ForStmt>(S);

    const Stmt *Init = FS->getInit();
    const Stmt *Cond = dyn_cast<Stmt>(FS->getCond());
    const Stmt *Inc = dyn_cast<Stmt>(FS->getInc());

    LoopAccess *LA = new LoopAccess;
    LA->LitLower = SIZE_MAX;
    LA->LitUpper = SIZE_MAX;
    LA->ExprLower = nullptr;
    LA->ExprUpper = nullptr;

    size_t InitLitBound = SIZE_MAX;
    size_t CondLitBound = SIZE_MAX;
    const Expr *InitExprBound = nullptr;
    const Expr *CondExprBound = nullptr;

    int IncType = 0;
    // Determine indexing variable
    if (Inc && isa<UnaryOperator>(Inc)) {
      const UnaryOperator *UO = dyn_cast<UnaryOperator>(Inc);
      if (UO->isIncrementDecrementOp()) {
        const DeclRefExpr *DRE = getLeftmostDecl(UO);
        LA->IndexDecl = DRE->getDecl();
        IncType = UO->isIncrementOp() - UO->isDecrementOp();
      }
    }

    // Determine bound from Init
    if (LA->IndexDecl && Init && isa<DeclStmt>(Init)) {
      const DeclStmt *DS = dyn_cast<DeclStmt>(Init);
      const Decl *D = DS->getSingleDecl();
      if (const VarDecl *VD = dyn_cast<VarDecl>(D)) {
        if (VD->getInit()->isEvaluatable(*Context)) {
          Expr::EvalResult Result;
          bool isConstant = VD->getInit()->EvaluateAsInt(Result, *Context);
          if (isConstant)
            InitLitBound = Result.Val.getInt().getExtValue();
        }
        if (InitLitBound == SIZE_MAX)
          InitExprBound = VD->getInit();
      }
    } else if (LA->IndexDecl && Init && isa<BinaryOperator>(Init)) {
      const BinaryOperator *BO = dyn_cast<BinaryOperator>(Init);
      if (BO->isAssignmentOp()) {
        if (BO->getRHS()->isEvaluatable(*Context)) {
          Expr::EvalResult Result;
          bool isConstant = BO->getRHS()->EvaluateAsInt(Result, *Context);
          if (isConstant)
            InitLitBound = Result.Val.getInt().getExtValue();
        }
        if (InitLitBound == SIZE_MAX)
          InitExprBound = BO->getRHS();
      }
    }

    // Determine bound from Cond
    if (LA->IndexDecl && Cond && isa<BinaryOperator>(Cond)) {
      const BinaryOperator *BO = dyn_cast<BinaryOperator>(Cond);
      if (BO->isComparisonOp()) {
        const Stmt *LHS = dyn_cast<Stmt>(BO->getLHS());
        const Stmt *RHS = dyn_cast<Stmt>(BO->getRHS());

        const DeclRefExpr *DRE = getLeftmostDecl(LHS);
        if (DRE && DRE->getDecl() == LA->IndexDecl) {
          // Index variable on LHS
          if (BO->getRHS()->isEvaluatable(*Context)) {
            Expr::EvalResult Result;
            bool isConstant = BO->getRHS()->EvaluateAsInt(Result, *Context);
            if (isConstant)
              CondLitBound = Result.Val.getInt().getExtValue();
          }
          if (CondLitBound == SIZE_MAX)
            CondExprBound = BO->getRHS();

        } else {
          DRE = getLeftmostDecl(RHS);
          if (DRE && DRE->getDecl() == LA->IndexDecl) {
            // Index variable on RHS
            if (BO->getLHS()->isEvaluatable(*Context)) {
              Expr::EvalResult Result;
              bool isConstant = BO->getLHS()->EvaluateAsInt(Result, *Context);
              if (isConstant)
                CondLitBound = Result.Val.getInt().getExtValue();
            }
            if (CondLitBound == SIZE_MAX)
              CondExprBound = BO->getLHS();
          }
        }

        // Determine comparison type for off by one compensation.
        if (IncType == 1) {
          LA->LowerOffByOne = 0;
          switch (BO->getOpcode()) {
          case BinaryOperator::Opcode::BO_LT:
          case BinaryOperator::Opcode::BO_GT:
          case BinaryOperator::Opcode::BO_NE:
            LA->UpperOffByOne = 0;
            break;
          case BinaryOperator::Opcode::BO_LE:
          case BinaryOperator::Opcode::BO_GE:
            LA->UpperOffByOne = 1;
            break;
          case BinaryOperator::Opcode::BO_EQ:
          default:
            break;
          }
        } else if (IncType == -1) {
          LA->UpperOffByOne = 1;
          switch (BO->getOpcode()) {
          case BinaryOperator::Opcode::BO_LT:
          case BinaryOperator::Opcode::BO_GT:
          case BinaryOperator::Opcode::BO_NE:
            LA->LowerOffByOne = 1;
            break;
          case BinaryOperator::Opcode::BO_LE:
          case BinaryOperator::Opcode::BO_GE:
            LA->LowerOffByOne = 0;
            break;
          case BinaryOperator::Opcode::BO_EQ:
          default:
            break;
          }
        }
      }
    }

    if (IncType == 1) {
      LA->LitLower = InitLitBound;
      LA->LitUpper = CondLitBound;
      LA->ExprLower = InitExprBound;
      LA->ExprUpper = CondExprBound;
    } else if (IncType == -1) {
      LA->LitLower = CondLitBound;
      LA->LitUpper = InitLitBound;
      LA->ExprLower = CondExprBound;
      LA->ExprUpper = InitExprBound;
    }

#if DEBUG_LEVEL >= 1
    llvm::outs() << LA->LitLower << " " << LA->LitUpper << "\n";
#endif
    if ((LA->LitLower != SIZE_MAX || LA->ExprLower) ||
        (LA->LitUpper != SIZE_MAX || LA->ExprUpper)) {
#if DEBUG_LEVEL >= 1
      llvm::outs() << "Committing loop bounds\n";
#endif
      NewEntry.LoopBounds = LA;
    } else {
      delete LA;
    }
  }

  NewEntry.Barrier = ScopeBarrier::LoopBegin;
  insertAccessLogEntry(NewEntry);
  NewEntry.Loc = S->getEndLoc();
  NewEntry.Barrier = ScopeBarrier::LoopEnd;
  insertAccessLogEntry(NewEntry);

  return 1;
}

int DataTracker::recordCond(const Stmt *S) {
  // Don't record if already accounted for. Each subcase of an IfStmt will be
  // seen here so this stops those from being doubled counted since we will
  // process each subcase when we encounter the first case.
  for (const Stmt *Temp : Conds) {
    if (Temp == S)
      return 0;
  }
  Conds.push_back(S);

  AccessInfo NewEntry = {};
  NewEntry.VD = nullptr;
  NewEntry.S = S;
  NewEntry.Loc = S->getBeginLoc();
  NewEntry.Flags = A_NOP;
  NewEntry.Barrier = ScopeBarrier::CondBegin;
  insertAccessLogEntry(NewEntry);

  if (const IfStmt *IF = dyn_cast<const IfStmt>(S)) {
    while (IF->hasElseStorage()) {
      const Stmt *Else = IF->getElse();
      SourceLocation Loc = IF->getElseLoc();
      IF = (dyn_cast<const IfStmt>(Else));
      if (IF) {
        // offset -1 removes ambiguity of position with beginning of the
        // conditional statement
        NewEntry.Loc = Loc;
        NewEntry.Barrier = ScopeBarrier::CondCase;
        insertAccessLogEntry(NewEntry);
      } else {
        // offset -1 removes ambiguity of position with beginning of the
        // else statement body
        NewEntry.Loc = Loc;
        NewEntry.Barrier = ScopeBarrier::CondFallback;
        insertAccessLogEntry(NewEntry);
        break;
      }
    }
  } else if (const SwitchStmt *SS = dyn_cast<const SwitchStmt>(S)) {
    const SwitchCase *Case = SS->getSwitchCaseList();
    do {
      if (isa<DefaultStmt>(Case))
        NewEntry.Barrier = ScopeBarrier::CondFallback;
      else
        NewEntry.Barrier = ScopeBarrier::CondCase;

      NewEntry.Loc = Case->getBeginLoc();
      NewEntry.Barrier = ScopeBarrier::CondCase;
      insertAccessLogEntry(NewEntry);

      Case = Case->getNextSwitchCase();
    } while (Case);

  } else {
#if DEBUG_LEVEL >= 1
    llvm::outs() << "Unknown Conditional Type.\n";
#endif
  }

  NewEntry.Loc = S->getEndLoc();
  NewEntry.Barrier = ScopeBarrier::CondEnd;
  insertAccessLogEntry(NewEntry);

  return 1;
}

int DataTracker::recordLocal(const ValueDecl *VD) {
  Locals.insert(VD);
  return 1;
}

int DataTracker::recordGlobal(const ValueDecl *VD) {
  Globals.insert(VD);
  return 1;
}

const std::vector<Kernel *> &DataTracker::getTargetRegions() const {
  return Kernels;
}

const std::vector<const CallExpr *> &DataTracker::getCallExprs() const {
  return CallExprs;
}

const std::vector<const Stmt *> &DataTracker::getLoops() const { return Loops; }

const TargetDataRegion *DataTracker::getTargetDataScope() const {
  return TargetScope;
}

const boost::container::flat_set<const ValueDecl *> &
DataTracker::getLocals() const {
  return Locals;
}

const boost::container::flat_set<const ValueDecl *> &
DataTracker::getGlobals() const {
  return Globals;
}

void DataTracker::classifyOffloadedOps() {
  SourceManager &SM = Context->getSourceManager();
  // Find target region bounds.
  for (Kernel *K : Kernels) {
    SourceLocation Lower = K->getBeginLoc();
    SourceLocation Upper = K->getEndLoc();
    auto It = AccessLog.begin();
    for (;
         SM.isBeforeInTranslationUnit(It->Loc, Lower) && It != AccessLog.end();
         ++It) {
    }
    K->AccessLogBegin = It;
    for (;
         SM.isBeforeInTranslationUnit(It->Loc, Upper) && It != AccessLog.end();
         ++It) {
    }
    K->AccessLogEnd = It;

    for (auto OffIt = K->AccessLogBegin; OffIt != K->AccessLogEnd; ++OffIt) {
      if (OffIt->Flags)
        OffIt->Flags |= A_OFFLD;
    }
  }

  return;
}

class VariableFinder : public RecursiveASTVisitor<VariableFinder> {
public:
  // explicit VariableFinder(ASTContext *Context) : Context(Context) {}

  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
      ReferencedVariables.insert(VD);
    }
    return true;
  }

  boost::container::flat_set<const VarDecl *> getReferencedVariables() const {
    return ReferencedVariables;
  }

private:
  // ASTContext *Context;
  boost::container::flat_set<const VarDecl *> ReferencedVariables;
};

/* Algorithm for determining placement of target update OpenMP directives for
 * array accesses in nested loops nested to arbitrary depth. LoopStack is a
 * stack that contains references to for statements.
 * The top of the stack corresponds to the for loop that includes array access
 * A in its body, with each subsequent element represents the enclosing loop.
 * insertionLocLim stores a statement which the directive must not proceed,
 * typically this is the end of the preceeding target kernel's scope.
 * Returns a pointer to the statement the directive should directly
 * proceed or follow.
 */
const AccessInfo *DataTracker::findOutermostIndexingLoop(
    std::vector<AccessInfo>::iterator &A,
    std::vector<const AccessInfo *> &LoopStack,
    std::vector<AccessInfo>::iterator &insertionLocLim) const {
  SourceManager &SM = Context->getSourceManager();
  const Expr *Idx = A->ArraySubscript->getIdx();
  // llvm::outs() << "Found var ArraySubscript ";
  // Idx->printPretty(llvm::outs(), nullptr, Context->getLangOpts());
  // llvm::outs() << "\n";
  VariableFinder Finder;
  Finder.TraverseStmt(const_cast<Expr *>(Idx));
  boost::container::flat_set<const VarDecl *> IndexingVars =
      Finder.getReferencedVariables();
  // llvm::outs() << "INDEXING VARS:";
  // for (const VarDecl *IndexingVar : IndexingVars) {
  //   llvm::outs() << " " << IndexingVar->getNameAsString() << ":" <<
  //   IndexingVar->getID();
  // }
  // llvm::outs() << "\n";

  const AccessInfo *OutermostIndexingLoop = nullptr;
  for (auto Rit = LoopStack.rbegin(); Rit != LoopStack.rend(); ++Rit) {
    if (insertionLocLim != AccessLog.end() &&
        SM.isBeforeInTranslationUnit((*Rit)->Loc, insertionLocLim->Loc))
      break;
    if (!(*Rit)->LoopBounds)
      continue;
    const ValueDecl *IndexValueDecl = (*Rit)->LoopBounds->IndexDecl;
    if (!IndexValueDecl)
      continue;
    const VarDecl *IndexVarDecl = dyn_cast<const VarDecl>(IndexValueDecl);
    if (!IndexVarDecl)
      continue;

    // llvm::outs() << "INDEXDECL:";
    // llvm::outs() << " " << IndexVarDecl->getNameAsString() << ":" <<
    // IndexVarDecl->getID() << "\n";
    if (!IndexingVars.contains(IndexVarDecl))
      continue;
    OutermostIndexingLoop = (*Rit);
    // llvm::outs() << "OutermostIndexingLoop:";
    // llvm::outs() << " " << OutermostIndexingLoop->Loc.printToString(SM) <<
    // "\n";
  }
  return OutermostIndexingLoop;
}

/* Finds the outermost Stmt in ContainingStmt that captures S. Returns nullptr
 * on error.
 */
const Stmt *DataTracker::findOutermostCapturingStmt(const Stmt *ContainingStmt,
                                                    const Stmt *S) const {
  const Stmt *CurrentStmt = S;
  while (true) {
    const auto &ImmediateParents = Context->getParents(*CurrentStmt);
    if (ImmediateParents.size() == 0)
      return nullptr;

    const Stmt *ParentStmt = ImmediateParents[0].get<Stmt>();
    if (!ParentStmt) {
      return CurrentStmt;
    }
    if (ParentStmt == ContainingStmt)
      return CurrentStmt;

    CurrentStmt = ParentStmt;
  }
}

/* Naive approach. Single target region analysis only.
 */
void DataTracker::naiveAnalyze() {
  classifyOffloadedOps();

  for (Kernel *K : Kernels) {
    // Seperate reads and writes into their own sets.
    for (auto It = K->AccessLogBegin; It != K->AccessLogEnd; ++It) {
      if (K->isPrivate(It->VD))
        continue;
      switch (It->Flags & 0b00000111) {
      case A_RDONLY:
        // Here we only want to map to if a variable was read before being
        // written to. This is more aggressive for arrays since the whole array
        // may not have been rewritten.
        if (!K->WriteDecls.contains(It->VD))
          K->ReadDecls.insert(It->VD);
        break;
      case A_WRONLY:
        K->WriteDecls.insert(It->VD);
        break;
      case A_UNKNOWN:
      case A_RDWR:
        K->ReadDecls.insert(It->VD);
        K->WriteDecls.insert(It->VD);
        break;
      case A_NOP:
      default:
        break;
      }
    }

    std::set_difference(K->ReadDecls.begin(), K->ReadDecls.end(),
                        K->WriteDecls.begin(), K->WriteDecls.end(),
                        std::inserter(K->MapTo, K->MapTo.begin()));
    std::set_difference(K->WriteDecls.begin(), K->WriteDecls.end(),
                        K->ReadDecls.begin(), K->ReadDecls.end(),
                        std::inserter(K->MapFrom, K->MapFrom.begin()));
    std::set_intersection(K->ReadDecls.begin(), K->ReadDecls.end(),
                          K->WriteDecls.begin(), K->WriteDecls.end(),
                          std::inserter(K->MapToFrom, K->MapToFrom.begin()));
  }

  return;
}

struct DataFlowOf {
  const ValueDecl *VD;
  DataFlowOf(const ValueDecl *VD) : VD(VD) {}
  bool operator()(const AccessInfo &Entry) {
    // Consider an entry in the access log to be in the flow of VD if the entry
    // contains control flow or is an access of VD.
    return Entry.Barrier || (Entry.VD == VD);
  }
};

void DataTracker::analyzeValueDecl(const ValueDecl *VD) {
  SourceManager &SM = Context->getSourceManager();
  bool MapTo = false;
  bool MapFrom = false;
  bool DataInitialized = false;
  bool DataValidOnHost = false;
  bool DataValidOnDevice = false;
  bool DataFirstPrivate = false;
  bool UsedInLastKernel = false;
  bool PrevMapTo = false;
  std::stack<LoopDependency> LoopDependencyStack;
  std::stack<CondDependency> CondDependencyStack;
  std::vector<ArrayAccess> ArrayBoundsList;
  std::vector<const AccessInfo *> LoopStack;
  std::vector<const AccessInfo *> PrevHostLoopStack;

  bool IsArithmeticType = VD->getType()->isArithmeticType();
  // bool isPointerType = VD->getType()->isAnyPointerType();
  bool MapAlloc = !IsArithmeticType; // arithmetic types can be transferred via
                                     // kernel parameters (firstprivate). all
                                     // others should be mapped. we may promote
                                     // alloc to to/from
  bool IsGlobal = Globals.contains(VD);
  bool IsParam = false;
  bool IsParamPtrToNonConst = false;
  std::vector<ParmVarDecl *> Params = FD->parameters();
  for (auto &Param : Params) {
    if (VD->getID() == Param->getID()) {
      IsParam = true;
      QualType ParamType = Param->getType();
      bool IsParamPtr =
          ParamType->isAnyPointerType() || ParamType->isReferenceType();
      IsParamPtrToNonConst = IsParamPtr && !isPtrOrRefToConst(ParamType);
      break;
    }
  }

  if (IsGlobal || IsParam) {
    DataInitialized = true;
    DataValidOnHost = true;
  }

#if DEBUG_LEVEL >= 1
  llvm::outs() << "Beginning Analysis of " << VD->getNameAsString() << "\n";
#endif

  auto PrevHostIt = AccessLog.end();
  auto PrevTgtIt = AccessLog.end();
  std::vector<AccessInfo>::iterator It;
  // Advance It to next access entry of VD or loop marker
  It = std::find_if(AccessLog.begin(), AccessLog.end(), DataFlowOf(VD));
  while (It != AccessLog.end()) {
    // Array Access Determination
    if (It->ArraySubscript) {
      ArrayAccess Bounds;
      Bounds.Flags = It->Flags;
      Bounds.LitLower = SIZE_MAX;
      Bounds.LitUpper = SIZE_MAX;
      const Expr *Idx = It->ArraySubscript->getIdx();
#if DEBUG_LEVEL >= 1
      llvm::outs() << "Found var ArraySubscript ";
      Idx->printPretty(llvm::outs(), nullptr, Context->getLangOpts());
      llvm::outs() << "\n";
#endif
      const ImplicitCastExpr *IdxICE = dyn_cast<ImplicitCastExpr>(Idx);
      const DeclRefExpr *IdxDRE =
          IdxICE ? dyn_cast<DeclRefExpr>(*(IdxICE->child_begin())) : nullptr;

      if (IdxDRE) {
        Bounds.VarLower = IdxDRE->getDecl();
        Bounds.VarUpper = Bounds.VarLower;
#if DEBUG_LEVEL >= 1
        llvm::outs() << "Found var: " << Bounds.VarLower->getNameAsString()
                     << "\n";
#endif
      } else if (Idx->isEvaluatable(*Context)) {
        Expr::EvalResult Result;
        bool isConstant = Idx->EvaluateAsInt(Result, *Context);
        if (isConstant) {
          Bounds.LitLower = Result.Val.getInt().getExtValue();
          Bounds.LitUpper = Bounds.LitLower;
        }
      }
      ArrayBoundsList.emplace_back(Bounds);
    }

    if (It->Barrier == ScopeBarrier::LoopBegin) {
      if (!(It->Flags & A_OFFLD)) {
        LoopDependency LD;
        LD.DataValidOnHost = DataValidOnHost;
        LD.DataValidOnDevice = DataValidOnDevice;
        LD.MapTo = MapTo;
        LD.FirstHostAccess = nullptr;
        LoopDependencyStack.emplace(LD);
      }
      LoopStack.push_back(&(*It));
    } else if (It->Barrier == ScopeBarrier::LoopEnd) {
      if (!(It->Flags & A_OFFLD)) {
        LoopDependency LD = LoopDependencyStack.top();
        if (LD.DataValidOnHost && !DataValidOnHost && LD.FirstHostAccess) {
          AccessInfo FirstAccess = *(LD.FirstHostAccess); // copy
          // Indicates to the rewriter that this statement is to be placed
          // directive directly before the loop end.
          FirstAccess.Barrier = ScopeBarrier::LoopEnd;
          TargetScope->UpdateFrom.emplace_back(FirstAccess);
          DataValidOnHost = true;
        }
        if ((LD.DataValidOnDevice && !DataValidOnDevice &&
             LD.FirstHostAccess) ||
            (!LD.MapTo && MapTo && !DataValidOnDevice)) {
          TargetScope->UpdateTo.emplace_back(*PrevHostIt);
          MapTo = LD.MapTo; // restore MapTo to before loop
        }
        LoopDependencyStack.pop();
      }
      LoopStack.pop_back();

    } else if (It->Barrier == ScopeBarrier::CondBegin) {
      CondDependency CD;
      CD.Conditional = *It;
      CD.Conditional.VD = VD;
      CondDependencyStack.emplace(CD);
    } else if (It->Barrier == ScopeBarrier::CondCase) {
    } else if (It->Barrier == ScopeBarrier::CondFallback) {
    } else if (It->Barrier == ScopeBarrier::CondEnd) {
      CondDependencyStack.pop();

    } else if (It->Barrier == ScopeBarrier::KernelBegin) {
      if (IsArithmeticType && !DataValidOnDevice) {
        DataFirstPrivate = true;
        UsedInLastKernel = false;
        PrevMapTo = MapTo;
      }
    } else if (It->Barrier == ScopeBarrier::KernelEnd) {
      if (IsArithmeticType && DataFirstPrivate) {
        // VD was a read-only scalar for this kernel and wasn't present already.
        // Undo data mappings and change to firsrprivate.
        if (!TargetScope->UpdateTo.empty() &&
            TargetScope->UpdateTo.back().Loc == PrevHostIt->Loc)
          TargetScope->UpdateTo.pop_back();
        MapTo = PrevMapTo;
        if (UsedInLastKernel)
          TargetScope->FirstPrivate.emplace_back(
              dyn_cast<OMPExecutableDirective>(It->S), VD);
        DataFirstPrivate = false;
        DataValidOnDevice = false;
      }
      PrevTgtIt = It;

    } else if (It->Flags & A_OFFLD) {
      // If not read-only then abandon attempt to classify as firstprivate.
      DataFirstPrivate &= (It->Flags == (A_RDONLY | A_OFFLD));

      if (!DataInitialized) {
        if (It->Flags & A_RDONLY) {
          // Read before write!
          DiagnosticsEngine &DiagEngine = Context->getDiagnostics();
          const unsigned int DiagID = DiagEngine.getCustomDiagID(
              DiagnosticsEngine::Warning,
              "variable '%0' is uninitialized when used here");
          DiagnosticBuilder DiagBuilder = DiagEngine.Report(It->Loc, DiagID);
          DiagBuilder.AddString(VD->getNameAsString());
        } else if ( // CondDependencyStack.empty()
                    //&&
            ((It->Flags == (A_WRONLY | A_OFFLD)) ||
             (It->Flags == (A_UNKNOWN | A_OFFLD)))) { // Write/Unknown
          DataInitialized = true;
        }
      } else if (
          // If data is written to but it is done so in a conditional statement,
          // copy to target device.
          (!CondDependencyStack.empty() &&
           (It->Flags & (A_WRONLY | A_UNKNOWN))) // Write/ReadWrite/Unknown
          ||
          // data is read, we need data present, copy to target device
          (!DataValidOnDevice &&
           (It->Flags & (A_RDONLY | A_UNKNOWN)))) { // Read/ReadWrite/Unknown
        // Data is already initalized, but not on target device
        if (PrevHostIt == AccessLog.end() ||
            SM.isBeforeInTranslationUnit(PrevHostIt->Loc,
                                         TargetScope->BeginLoc)) {
          // PrevHostIt == AccessLog.end() indicates the first access of a
          // global or parameter on the target device.
          MapTo = true;

        } else if (PrevHostIt->ArraySubscript) {
          const AccessInfo *OutermostIndexingLoop = findOutermostIndexingLoop(
              PrevHostIt, PrevHostLoopStack, PrevTgtIt);
          if (OutermostIndexingLoop) {
            AccessInfo OutermostIndexingLoopCopy =
                *OutermostIndexingLoop; // copy
            OutermostIndexingLoopCopy.VD = PrevHostIt->VD;
            // indicates to the rewriter that this statement should be placed
            // directly after the end of the loop
            OutermostIndexingLoopCopy.Barrier = ScopeBarrier::LoopEnd;
            TargetScope->UpdateTo.emplace_back(OutermostIndexingLoopCopy);
          } else {
            // does not depend on loop index.
            TargetScope->UpdateTo.emplace_back(*PrevHostIt);
          }

        } else {
          TargetScope->UpdateTo.emplace_back(*PrevHostIt);
        }
        DataValidOnDevice = true;
      }
      if ((It->Flags & (A_WRONLY | A_UNKNOWN))) { // Write/ReadWrite/Unknown
        DataValidOnDevice = true;
        DataValidOnHost = false;
        // a single write on the target guarantees we need to at allocate space
        MapAlloc = true;
      }

      UsedInLastKernel = true;

      // end check access on target device
    } else {
      // check access on host
      if (!DataInitialized) {
        if (It->Loc == It->VD->getLocation() &&
            !SM.isBeforeInTranslationUnit(It->Loc, TargetScope->BeginLoc)) {
          // Data needs to be declared before the target scope in which it is
          // used.
          DiagnosticsEngine &DiagEngine = Context->getDiagnostics();
          const unsigned int DiagID = DiagEngine.getCustomDiagID(
              DiagnosticsEngine::Warning,
              "declaration of '%0' is captured within a target data region in "
              "which it is being utilized");
          const unsigned int NoteID = DiagEngine.getCustomDiagID(
              DiagnosticsEngine::Note,
              "declaration of '%0' was anticipated to precede the beginning of "
              "the target data region at this location");
          DiagEngine.Report(It->Loc, DiagID) << VD->getNameAsString();
          DiagEngine.Report(TargetScope->BeginLoc, NoteID)
              << VD->getNameAsString();
        }
        if (It->Flags & A_RDONLY) {
          // Read before write!
          DiagnosticsEngine &DiagEngine = Context->getDiagnostics();
          const unsigned int DiagID = DiagEngine.getCustomDiagID(
              DiagnosticsEngine::Warning,
              "variable '%0' is uninitialized when used here");
          DiagEngine.Report(It->Loc, DiagID) << VD->getNameAsString();
        } else if ((It->Flags == A_WRONLY) ||
                   (It->Flags == A_UNKNOWN)) { // Write/Unknown
          DataInitialized = true;
        }
      } else if (!DataValidOnHost &&
                 (It->Flags &
                  (A_RDONLY | A_UNKNOWN))) { // Read/ReadWrite/Unknown
        if (SM.isBeforeInTranslationUnit(TargetScope->EndLoc, It->Loc)) {
          MapFrom = true;
        } else if (It->ArraySubscript) {
          const AccessInfo *OutermostIndexingLoop =
              findOutermostIndexingLoop(It, LoopStack, PrevTgtIt);
          if (OutermostIndexingLoop) {
            AccessInfo OutermostIndexingLoopCopy =
                *OutermostIndexingLoop; // copy
            OutermostIndexingLoopCopy.VD = It->VD;
            TargetScope->UpdateFrom.emplace_back(OutermostIndexingLoopCopy);
          } else {
            // does not depend on loop index.
            if (!CondDependencyStack.empty()) {
              TargetScope->UpdateFrom.emplace_back(
                  CondDependencyStack.top().Conditional);
            } else {
              TargetScope->UpdateFrom.emplace_back(*It);
            }
          }

        } else if (!CondDependencyStack.empty()) {
          TargetScope->UpdateFrom.emplace_back(
              CondDependencyStack.top().Conditional);
        } else {
          TargetScope->UpdateFrom.emplace_back(*It);
        }
        DataValidOnHost = true;
      }
      if (It->Flags & (A_WRONLY | A_UNKNOWN)) { // Write/ReadWrite/Unknown
        DataValidOnDevice = false;
        DataValidOnHost = true;
      }
      if (!LoopDependencyStack.empty() &&
          LoopDependencyStack.top().FirstHostAccess == nullptr) {
        LoopDependencyStack.top().FirstHostAccess = &(*It);
      }
      PrevHostIt = It;
      PrevHostLoopStack = LoopStack;
    } // end check access on host

    // Advance It to next access entry of VD
    ++It;
    It = std::find_if(It, AccessLog.end(), DataFlowOf(VD));
  } // end while

  // if ( (VD is a (non const point parameter of the function) || VD is a
  // (global)) && !dataLastOnHost)
  if ((IsGlobal || IsParamPtrToNonConst) && !DataValidOnHost) {
    MapFrom = true;
    DataValidOnHost = true;
  }

  AccessInfo Access;
  Access.VD = VD;
  if (MapTo && MapFrom)
    TargetScope->MapToFrom.push_back(Access);
  else if (MapTo)
    TargetScope->MapTo.push_back(Access);
  else if (MapFrom)
    TargetScope->MapFrom.push_back(Access);
  else if (MapAlloc)
    TargetScope->MapAlloc.push_back(Access);

  return;
}

void DataTracker::analyze() {
  AccessInfo *firstOffload = nullptr;
  AccessInfo *lastOffload = nullptr;
  for (AccessInfo &Access : AccessLog) {
    if (Access.Flags & A_OFFLD) {
      if (!firstOffload) {
        firstOffload = &Access;
        lastOffload = &Access;
      } else {
        lastOffload = &Access;
      }
    }
  }
  if (!firstOffload) {
    // There is nothing to analyze.
    return;
  }

  classifyOffloadedOps();

  const Stmt *FrontCapturingStmt =
      findOutermostCapturingStmt(FD->getBody(), firstOffload->S);
  const Stmt *BackCapturingStmt =
      findOutermostCapturingStmt(FD->getBody(), lastOffload->S);
  SourceLocation ScopeBegin;
  SourceLocation ScopeEnd;
  if (FrontCapturingStmt) {
    ScopeBegin = FrontCapturingStmt->getBeginLoc();
#if DEBUG_LEVEL >= 1
    llvm::outs() << "FrontCapturingStmt is "
                 << ScopeBegin.printToString(Context->getSourceManager())
                 << " in " << FD->getNameAsString() << "\n";
#endif
  } else {
#if DEBUG_LEVEL >= 1
    llvm::outs() << "FrontCapturingStmt null in " << FD->getNameAsString()
                 << "\n";
#endif
  }
  if (BackCapturingStmt) {
    ScopeEnd = BackCapturingStmt->getEndLoc();
#if DEBUG_LEVEL >= 1
    llvm::outs() << "BackCapturingStmt is "
                 << ScopeEnd.printToString(Context->getSourceManager())
                 << " in " << FD->getNameAsString() << "\n";
#endif
  } else {
#if DEBUG_LEVEL >= 1
    llvm::outs() << "BackCapturingStmt null in " << FD->getNameAsString()
                 << "\n";
#endif
  }

  if (Kernels.size() > 0) {
    // Identify the root(outermost) target scope in the function.
    FrontCapturingStmt =
        findOutermostCapturingStmt(FD->getBody(), Kernels[0]->getDirective());
    SourceLocation KernelScopeBegin = FrontCapturingStmt->getBeginLoc();
    BackCapturingStmt = findOutermostCapturingStmt(
        FD->getBody(), Kernels.back()->getDirective());
    SourceLocation KernelScopeEnd = BackCapturingStmt->getEndLoc();
    if (auto OpenMPDirective =
            dyn_cast<OMPExecutableDirective>(BackCapturingStmt)) {
      // ugh. The getEndLoc() of OpenMP directives is different and doesn't
      // consider the captured statement while seemingly every other statement
      // does.
      KernelScopeEnd = OpenMPDirective->getInnermostCapturedStmt()->getEndLoc();
    }
    SourceManager &SM = Context->getSourceManager();
    if (ScopeBegin.isInvalid() ||
        SM.isBeforeInTranslationUnit(KernelScopeBegin, ScopeBegin)) {
      ScopeBegin = KernelScopeBegin;
    }
    if (ScopeEnd.isInvalid() ||
        SM.isBeforeInTranslationUnit(ScopeEnd, KernelScopeEnd)) {
      ScopeEnd = KernelScopeEnd;
    }
  }

  if (ScopeBegin.isInvalid() || ScopeEnd.isInvalid()) {
    llvm::outs()
        << "error: Data mapping scope could not be determined for function "
        << FD->getNameAsString() << "\n";
    return;
  }

  TargetScope = new TargetDataRegion(ScopeBegin, ScopeEnd, FD);

  for (Kernel *K : Kernels) {
    TargetScope->Kernels.push_back(K->getDirective());
  }

  // Map a list of all the data the TargetScope will be responsible for.
  boost::container::flat_set<const ValueDecl *> TargetScopeDecls;
  for (auto It = AccessLog.begin(); It != AccessLog.end(); ++It) {
    if (It->Flags & A_OFFLD && It->Barrier == ScopeBarrier::None)
      TargetScopeDecls.insert(It->VD);
  }

  for (Kernel *K : Kernels) {
    auto Privates = K->getPrivateDecls();
    for (const ValueDecl *VD : Privates) {
      Disabled.insert(VD->getID());
    }
  }

  for (const ValueDecl *VD : TargetScopeDecls) {
    if (!Disabled.contains(VD->getID()))
      analyzeValueDecl(VD);
  }

  return;
}

std::vector<uint8_t> DataTracker::getParamAccessModes(bool crossFnOffloading) {
  std::vector<uint8_t> results;
  std::vector<ParmVarDecl *> Params = FD->parameters();
  if (Params.size() == 0)
    return results;

  for (int I = 0; I < FD->getNumParams(); ++I) {
    QualType ParamType = FD->getParamDecl(I)->getType();
    // shortcut since we wont care about passed by value
    if (!ParamType->isPointerType() && !ParamType->isReferenceType()) {
      results.push_back(A_NOP);
      continue;
    }

    int Flags = A_NOP;
    int OffldOnly = A_OFFLD;
    for (AccessInfo Entry : AccessLog) {
      if (Entry.VD && Entry.VD->getID() == Params[I]->getID()) {
        Flags |= Entry.Flags;
        OffldOnly &= Flags;
      }
    }
    if (!crossFnOffloading || !OffldOnly) {
      // clear offloaded flag
      Flags &= ~A_OFFLD;
    } else {
      // data for this variable will be managed by caller.
      // this is an aggressive assumption since this function may be called by
      // another translation unit, so doing this could be unsafe.
      Disabled.insert(Params[I]->getID());
    }

    results.push_back(Flags);
  }
  return results;
}

std::vector<uint8_t> DataTracker::getGlobalAccessModes(bool crossFnOffloading) {
  std::vector<uint8_t> results;
  if (Globals.size() == 0)
    return results;

  for (const ValueDecl *Global : Globals) {
    int Flags = A_NOP;
    int OffldOnly = A_OFFLD;
    for (AccessInfo Entry : AccessLog) {
      if (Entry.VD && Entry.VD->getID() == Global->getID()) {
        Flags |= Entry.Flags;
        OffldOnly &= Flags;
      }
    }
    if (!crossFnOffloading || !OffldOnly) {
      // clear offloaded flag
      Flags &= ~A_OFFLD;
    } else {
      // data for this variable will be managed by caller.
      // this is an aggressive assumption since this function may be called by
      // another translation unit, so doing this could be unsafe.
      Disabled.insert(Global->getID());
    }
    results.push_back(Flags);
  }
  return results;
}

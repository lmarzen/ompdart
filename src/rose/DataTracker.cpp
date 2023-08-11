#include "DataTracker.h"

#include "clang/AST/ParentMapContext.h"

#include "CommonUtils.h"

using namespace clang;

struct LoopDependency {
  bool DataValidOnHost;
  bool DataValidOnDevice;
  bool MapTo;
  AccessInfo *FirstHostAccess;
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

const FunctionDecl *DataTracker::getDecl() const {
  return FD;
}

bool DataTracker::contains(SourceLocation Loc) const {
  Stmt *Body = FD->getBody();
  SourceLocation BodyBeginLoc = Body->getBeginLoc();
  SourceLocation BodyEndLoc = Body->getEndLoc();
  return (BodyBeginLoc <= Loc) && (Loc < BodyEndLoc);
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
  do {
    --It;
  } while (NewEntry.Loc < It->Loc);

  // insert at correct position in AccessLog
  ++It;
  AccessLog.insert(It, NewEntry);
  return 1;
}

int DataTracker::recordAccess(const ValueDecl *VD,
                              SourceLocation Loc,
                              const Stmt *S,
                              uint8_t Flags,
                              bool overwrite) {
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

    if (LastKernel->getDirective()->getBeginLoc() <= VD->getBeginLoc()
     && VD->getBeginLoc() < LastKernel->getDirective()->getEndLoc())
      return 0;

    if (LastKernel->getDirective()->getBeginLoc() <= Loc
     && Loc < LastKernel->getDirective()->getEndLoc())
      return 0;

    if (LastKernel->NestedDirectives.size() > 0) {
      auto *NestedDir = LastKernel->NestedDirectives.back();
      if (NestedDir->getBeginLoc() <= Loc
       && Loc < NestedDir->getEndLoc())
        return 0;
    }

    if (LastKernel->contains(Loc))
      Flags |= A_OFFLD;
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
  NewEntry.VD      = VD;
  NewEntry.S       = S;
  NewEntry.Loc     = Loc;
  NewEntry.Flags   = Flags;

  if (VD && VD == LastArrayBasePointer) {
    NewEntry.ArraySubscript = LastArraySubscript;
    LastArrayBasePointer    = nullptr;
    LastArraySubscript      = nullptr;
  }

  return insertAccessLogEntry(NewEntry);
}

const std::vector<AccessInfo> &DataTracker::getAccessLog() {
  return AccessLog;
}

/* Update reads/writes that may have happened on by the Callee parameters passed
 * by pointer.
*/
int DataTracker::updateParamsTouchedByCallee(const FunctionDecl *Callee,
                                             const std::vector<const CallExpr *> &Calls,
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
      const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Args[I]->IgnoreImpCasts());
      if (!DRE) {
        // is a literal
        continue;
      }
      const ValueDecl *VD = DRE->getDecl();
      numUpdates += recordAccess(VD, DRE->getLocation(), nullptr, ParamModes[I], true);
    }
  }
  return numUpdates;
}

/* Update reads/writes that may have occurred by the Callee on global variables.
 * We will need to insert these into our own AccessLog.
 */
int DataTracker::updateGlobalsTouchedByCallee(const FunctionDecl *Callee,
                                              const std::vector<const CallExpr *> &Calls,
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
      numUpdates += recordAccess(Global, CE->getBeginLoc(), nullptr, GlobalModes[I], true);
    }
    recordGlobal(Global);
    ++I;
  }
  
  return numUpdates;
}

int DataTracker::updateTouchedByCallee(const FunctionDecl *Callee,
                                       const std::vector<uint8_t> &ParamModes,
                                       const boost::container::flat_set<const ValueDecl *> &GlobalsAccessed,
                                       const std::vector<uint8_t> &GlobalModes) {
  int numUpdates = 0;

  // Start by finding all the calls to the callee.
  std::vector<const CallExpr *> Calls;
  for (const CallExpr *CE : CallExprs) {
    if (CE->getDirectCallee()->getDefinition() == Callee)
      Calls.push_back(CE);
  }

  if (Calls.size() == 0)
    return numUpdates;

  numUpdates += updateGlobalsTouchedByCallee(Callee, Calls, GlobalsAccessed, GlobalModes);
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
    // llvm::outs() << " " << Entry.VD->getID();
    ExtraSpace =  WidthVD - Entry.VD->getNameAsString().size() + 1;

    llvm::outs() << std::string(ExtraSpace + 1, ' ');
    switch (Entry.Flags & (A_RDWR | A_UNKNOWN))
    {
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
                           return A.VD == BasePointer 
                                  && A.Loc == Subscript->getBeginLoc();
                         });

  if (It == AccessLog.end()) {
    // We have parsed the array subscript before determining the access type.
    // Save it so it can be attached when the access is record in the access
    // log.
    LastArrayBasePointer = BasePointer;
    LastArraySubscript   = Subscript;
    return 1;
  }
  
  It->ArraySubscript = Subscript;
  LastArrayBasePointer = nullptr;
  LastArraySubscript   = nullptr;
  return 1;
}

int DataTracker::recordTargetRegion(Kernel *K) {
  LastKernel = K;
  Kernels.push_back(K);

  AccessInfo NewEntry = {};
  NewEntry.VD      = nullptr;
  NewEntry.S       = K->getDirective();
  NewEntry.Loc     = K->getBeginLoc();
  NewEntry.Flags   = A_NOP;
  NewEntry.Barrier = ScopeBarrier::KernelBegin;
  insertAccessLogEntry(NewEntry);
  NewEntry.Loc     = K->getEndLoc();
  NewEntry.Barrier = ScopeBarrier::KernelEnd;
  insertAccessLogEntry(NewEntry);
  return 1;
}

int DataTracker::recordCallExpr(const CallExpr *CE) {
  CallExprs.push_back(CE);
  return 1;
}

int DataTracker::recordLoop(const Stmt *S) {
  // Don't record loops inside a target region
  if (LastKernel != nullptr && LastKernel->contains(S->getBeginLoc()))
    return 0;

  Loops.push_back(S);

  AccessInfo NewEntry = {};
  NewEntry.VD      = nullptr;
  NewEntry.S       = S;
  NewEntry.Loc     = S->getBeginLoc();
  NewEntry.Flags   = A_NOP;
  NewEntry.Barrier = ScopeBarrier::LoopBegin;
  insertAccessLogEntry(NewEntry);
  NewEntry.Loc     = S->getEndLoc();
  NewEntry.Barrier = ScopeBarrier::LoopEnd;
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

const std::vector<const Stmt *> &DataTracker::getLoops() const {
  return Loops;
}

const TargetDataRegion *DataTracker::getTargetDataScope() const {
  return TargetScope;
}

const boost::container::flat_set<const ValueDecl *> &DataTracker::getLocals() const {
  return Locals;
}

const boost::container::flat_set<const ValueDecl *> &DataTracker::getGlobals() const {
  return Globals;
}

/* Finds the outermost Stmt in ContainingStmt that captures S. Returns nullptr
 * on error.
 */
const Stmt *DataTracker::findOutermostCapturingStmt(const Stmt *ContainingStmt,
                                                    const Stmt *S) {
  const Stmt *CurrentStmt = S;
  while (true) {
    const auto &ImmediateParents = Context->getParents(*CurrentStmt);
    if (ImmediateParents.size() == 0)
      return nullptr;

    const Stmt *ParentStmt = ImmediateParents[0].get<Stmt>();
    if (ParentStmt == ContainingStmt)
      return CurrentStmt;

    CurrentStmt = ParentStmt;
  }
}

/* Naive approach. Single target region analysis only.
 */
void DataTracker::naiveAnalyze() {
  // Find target region bounds.
  for (Kernel *K : Kernels) {
    AccessInfo Lower, Upper;
    Lower.Loc = K->getBeginLoc();
    Upper.Loc = K->getEndLoc();
    K->AccessLogBegin = std::lower_bound(AccessLog.begin(), AccessLog.end(), Lower);
    K->AccessLogEnd   = std::upper_bound(AccessLog.begin(), AccessLog.end(), Upper);
  }

  for (Kernel *K : Kernels) {
    // Seperate reads and writes into their own sets.
    for (auto It = K->AccessLogBegin; It != K->AccessLogEnd; ++It) {
      switch (It->Flags & 0b00000111)
      {
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
    // contains info for VD or is a the beginning or end of a loop or kernel.
    return Entry.Barrier || (Entry.VD == VD);
  }
};

void DataTracker::analyzeValueDecl(const ValueDecl *VD) {
  bool MapTo = false;
  bool MapFrom = false;
  bool DataInitialized = false;
  bool DataValidOnHost = false;
  bool DataValidOnDevice = false;
  bool DataFirstPrivate = false;
  bool UsedInLastKernel = false;
  std::stack<LoopDependency> LoopDependencyStack;

  bool IsArithmeticType = VD->getType()->isArithmeticType();
  bool MapAlloc = !IsArithmeticType;
  bool IsGlobal = Globals.contains(VD);
  bool IsParam = false;
  bool IsParamPtrToNonConst = false;
  std::vector<ParmVarDecl *> Params = FD->parameters();
  for (auto &Param : Params) {
    if (VD->getID() == Param->getID()) {
      IsParam = true;
      QualType ParamType = Param->getType();
      bool IsParamPtr = ParamType->isAnyPointerType()
                        || ParamType->isReferenceType();
      IsParamPtrToNonConst = IsParamPtr && !isPtrOrRefToConst(ParamType);
      break;
    }
  }
  
  if (IsGlobal || IsParam) {
    DataInitialized = true;
    DataValidOnHost = true;
  }

  auto PrevHostIt = AccessLog.end();
  std::vector<AccessInfo>::iterator It;
  // Advance It to next access entry of VD or loop marker
  It = std::find_if(AccessLog.begin(), AccessLog.end(), DataFlowOf(VD));
  while (It != AccessLog.end()) {
    if (It->Barrier == ScopeBarrier::LoopBegin) {
      LoopDependency LD;
      LD.DataValidOnHost   = DataValidOnHost;
      LD.DataValidOnDevice = DataValidOnDevice;
      LD.MapTo             = MapTo;
      LD.FirstHostAccess   = nullptr;
      LoopDependencyStack.emplace(LD);
    } else if (It->Barrier == ScopeBarrier::LoopEnd) {
      LoopDependency LD = LoopDependencyStack.top();
      if (LD.DataValidOnHost && !DataValidOnHost && LD.FirstHostAccess) {
        AccessInfo FirstAccess = *(LD.FirstHostAccess); // copy
        // Indicates to the rewriter that this statement is to be placed
        // directive directly before the loop end.
        FirstAccess.Barrier = ScopeBarrier::LoopEnd; 
        TargetScope->UpdateFrom.emplace_back(FirstAccess);
      }
      if ((LD.DataValidOnDevice && !DataValidOnDevice && LD.FirstHostAccess)
       || (!LD.MapTo && MapTo && !DataValidOnDevice)) {
        TargetScope->UpdateTo.emplace_back(*PrevHostIt);
        MapTo = LD.MapTo; // restore MapTo to before loop
      }
      LoopDependencyStack.pop();

    } else if (It->Barrier == ScopeBarrier::KernelBegin) {
      if (IsArithmeticType && !DataValidOnDevice) {
        DataFirstPrivate = true;
        DataValidOnDevice = true;
        UsedInLastKernel = false;
      }
    } else if (It->Barrier == ScopeBarrier::KernelEnd) {
      if (IsArithmeticType && DataFirstPrivate) {
        DataFirstPrivate = false;
        DataValidOnDevice = false;
        if (UsedInLastKernel)
          TargetScope->FirstPrivate.emplace_back(
              dyn_cast<OMPExecutableDirective>(It->S), VD);
      }

    } else if (It->Flags & A_OFFLD) {
      // check access on target device
      if (DataFirstPrivate && (It->Flags != (A_RDONLY  | A_OFFLD))) { // ReadOnly
        // If not read-only then abandon attempt to classify as firstprivate.
        DataFirstPrivate = false;
        DataValidOnDevice = false;
      }
      if (!DataInitialized) {
        if (It->Flags & A_RDONLY) {
          // Read before write!
          DiagnosticsEngine &DiagEngine = Context->getDiagnostics();
          const unsigned int DiagID = DiagEngine.getCustomDiagID(
              DiagnosticsEngine::Warning,
              "variable '%0' is uninitialized when used here");
          DiagnosticBuilder DiagBuilder = DiagEngine.Report(It->Loc, DiagID);
          DiagBuilder.AddString(VD->getNameAsString());
        } else if ((It->Flags == (A_WRONLY  | A_OFFLD))
                || (It->Flags == (A_UNKNOWN | A_OFFLD))) { // Write/Unknown
          DataInitialized = true;
        }
      } else if (!DataValidOnDevice && (It->Flags & (A_RDONLY | A_UNKNOWN))) { // Read/ReadWrite/Unknown
        // Data is already initalized, but not on target device
        if (PrevHostIt == AccessLog.end()
         || PrevHostIt->Loc < TargetScope->BeginLoc) {
          // PrevHostIt == AccessLog.end() indicates the first access of a
          // global or parameter on the target device.
          MapTo = true;
        } else {
          TargetScope->UpdateTo.emplace_back(*PrevHostIt);
        }
        DataValidOnDevice = true;
      }
      if (It->Flags & (A_WRONLY | A_UNKNOWN)) {// Write/ReadWrite/Unknown
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
        if (It->Loc == It->VD->getLocation()
         && TargetScope->BeginLoc <= It->Loc) {
          // Data needs to be declared before the target scope in which it is
          // used.
          DiagnosticsEngine &DiagEngine = Context->getDiagnostics();
          const unsigned int DiagID = DiagEngine.getCustomDiagID(
              DiagnosticsEngine::Error,
              "declaration of '%0' is captured within a target data region in which it is being utilized");
           const unsigned int NoteID = DiagEngine.getCustomDiagID(
              DiagnosticsEngine::Note,
              "declaration of '%0' was anticipated to precede the beginning of the target data region at this location");
          DiagEngine.Report(It->Loc, DiagID) << VD->getNameAsString();
          DiagEngine.Report(TargetScope->BeginLoc, NoteID) << VD->getNameAsString();
        }
        if (It->Flags & A_RDONLY) {
          // Read before write!
          DiagnosticsEngine &DiagEngine = Context->getDiagnostics();
          const unsigned int DiagID = DiagEngine.getCustomDiagID(
              DiagnosticsEngine::Warning,
              "variable '%0' is uninitialized when used here");
          DiagEngine.Report(It->Loc, DiagID) << VD->getNameAsString();
        }
        else if ((It->Flags == A_WRONLY)
              || (It->Flags == A_UNKNOWN)) { // Write/Unknown
          DataInitialized = true;
        }
      }
      else if (!DataValidOnHost && (It->Flags & (A_RDONLY | A_UNKNOWN))) { // Read/ReadWrite/Unknown
        if (TargetScope->EndLoc < It->Loc) {
          MapFrom = true;
        } else {
          TargetScope->UpdateFrom.emplace_back(*It);
        }
        DataValidOnHost = true;
      }
      if (It->Flags & (A_WRONLY | A_UNKNOWN)) { // Write/ReadWrite/Unknown
        DataValidOnDevice = false;
        DataValidOnHost = true;
      }
      if (!LoopDependencyStack.empty()
       && LoopDependencyStack.top().FirstHostAccess == nullptr) {
        LoopDependencyStack.top().FirstHostAccess = &(*It);
      }
      PrevHostIt = It;
    } // end check access on host

    // Advance It to next access entry of VD
    ++It;
    It = std::find_if(It, AccessLog.end(), DataFlowOf(VD));
  } // end while

  //if ( (VD is a (non const point parameter of the function) || VD is a (global)) && !dataLastOnHost)
  if ((IsGlobal || IsParamPtrToNonConst) && !DataValidOnHost) {
    MapFrom = true;
    DataValidOnHost = true;
  }

  if (MapTo && MapFrom)
    TargetScope->MapToFrom.push_back(VD);
  else if (MapTo)
    TargetScope->MapTo.push_back(VD);
  else if (MapFrom)
    TargetScope->MapFrom.push_back(VD);
  else if (MapAlloc)
    TargetScope->MapAlloc.push_back(VD);

  return;
}

void DataTracker::analyze() {
  if (Kernels.size() == 0) {
    // There is nothing to analyze.
    return;
  }

  // Identify the root(outermost) target scope in the function.
  const Stmt *FrontCapturingStmt = findOutermostCapturingStmt(FD->getBody(), Kernels[0]->getDirective());
  SourceLocation ScopeBegin = FrontCapturingStmt->getBeginLoc();
  const Stmt *BackCapturingStmt  = findOutermostCapturingStmt(FD->getBody(), Kernels.back()->getDirective());
  SourceLocation ScopeEnd = BackCapturingStmt->getEndLoc();
  if (auto OpenMPDirective = dyn_cast<OMPExecutableDirective>(BackCapturingStmt)) {
    // ugh. The getEndLoc() of OpenMP directives is different and doesn't
    // consider the captured statement while seemingly every other statement
    // does.
    ScopeEnd = OpenMPDirective->getInnermostCapturedStmt()->getEndLoc();
  }

  TargetScope = new TargetDataRegion(ScopeBegin, ScopeEnd, FD);

  for (Kernel *K : Kernels) {
    TargetScope->Kernels.push_back(K->getDirective());
  }

  // Map a list of all the data the TargetScope will be responsible for.
  boost::container::flat_set<const ValueDecl *> TargetScopeDecls;
  for (auto It = AccessLog.begin(); It != AccessLog.end(); ++It) {
    if (It->Flags & A_OFFLD)
      TargetScopeDecls.insert(It->VD);
  }

  for (const ValueDecl *VD : TargetScopeDecls) {
    analyzeValueDecl(VD);
  }

  return;
}


void analyzeValueDeclArrayBounds(const ValueDecl *VD) {

  return;
}

void DataTracker::analyzeArrayBounds() {


  return;
}

std::vector<uint8_t> DataTracker::getParamAccessModes() const {
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
    for (AccessInfo Entry : AccessLog) {
      if (Entry.VD && Entry.VD->getID() == Params[I]->getID())
        Flags |= Entry.Flags;
    }
    // clear offloaded flag
    Flags &= ~A_OFFLD;

    results.push_back(Flags);
  }
  return results;
}

std::vector<uint8_t> DataTracker::getGlobalAccessModes() const {
  std::vector<uint8_t> results;
  if (Globals.size() == 0)
    return results;

  for (const ValueDecl *Global : Globals) {
    int Flags = A_NOP;
    for (AccessInfo Entry : AccessLog) {
      if (Entry.VD && Entry.VD->getID() == Global->getID())
        Flags |= Entry.Flags;
    }
    // clear offloaded flag
    Flags &= ~A_OFFLD;
    results.push_back(Flags);
  }
  return results;
}
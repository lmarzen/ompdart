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
  this->LastTargetRegion = NULL;
  this->TargetScope = NULL;
};

FunctionDecl *DataTracker::getDecl() const {
  return FD;
}

bool DataTracker::contains(SourceLocation Loc) const {
  Stmt *Body = FD->getBody();
  SourceLocation BodyBeginLoc = Body->getBeginLoc();
  SourceLocation BodyEndLoc = Body->getEndLoc();
  return (BodyBeginLoc <= Loc) && (Loc <= BodyEndLoc);
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

int DataTracker::recordAccess(ValueDecl *VD, SourceLocation Loc, Stmt *S,
                              uint8_t Flags, bool overwrite) {
  if (LastTargetRegion) {
    // Don't record private data.
    if (LastTargetRegion->isPrivate(VD))
      return 0;

    // Clang duplicates references when they are declared in function parameters
    // and then used in openmp target regions. These can be filtered out easily
    // as the duplicate will have the same location as the directive statement.
    // Additionally, clang may create temporary variables when using openmp
    // target directives. These can be filtered out by ignoring all variables
    // that have names that begin with a period(.).
    if (LastTargetRegion->getDirective()->getBeginLoc() == VD->getBeginLoc()
     || VD->getNameAsString()[0] == '.') {
      llvm::outs() << "\nIgnoring: " << VD->getNameAsString() << " " << VD->getID();
      return 0;
    }

    if (LastTargetRegion->contains(Loc)) 
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

  AccessInfo NewEntry;
  NewEntry.VD    = VD;
  NewEntry.S     = S;
  NewEntry.Loc   = Loc;
  NewEntry.Flags = Flags;

  return insertAccessLogEntry(NewEntry);
}

const std::vector<AccessInfo> &DataTracker::getAccessLog() {
  return AccessLog;
}

/* Update reads/writes that may have happened on by the Callee parameters passed
 * by pointer.
*/
int DataTracker::updateParamsTouchedByCallee(FunctionDecl *Callee,
                                             const std::vector<CallExpr *> &Calls,
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

  for (CallExpr *CE : Calls) {
    Expr **Args = CE->getArgs();

    for (int I = 0; I < Callee->getNumParams(); ++I) {
      QualType ParamType = Callee->getParamDecl(I)->getType();
      if (!ParamType->isPointerType() && !ParamType->isReferenceType())
        continue;
      DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Args[I]->IgnoreImpCasts());
      if (!DRE) {
        // is a literal
        continue;
      }
      ValueDecl *VD = DRE->getDecl();
      numUpdates += recordAccess(VD, DRE->getLocation(), nullptr, ParamModes[I], true);
    }
  }
  return numUpdates;
}

/* Update reads/writes that may have occurred by the Callee on global variables.
 * We will need to insert these into our own AccessLog.
 */
int DataTracker::updateGlobalsTouchedByCallee(FunctionDecl *Callee,
                                              const std::vector<CallExpr *> &Calls,
                                              const boost::container::flat_set<ValueDecl *> &GlobalsAccessed,
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
  for (ValueDecl *Global : GlobalsAccessed) {
    for (CallExpr *CE : Calls) {
      numUpdates += recordAccess(Global, CE->getBeginLoc(), nullptr, GlobalModes[I], true);
    }
    recordGlobal(Global);
    ++I;
  }
  
  return numUpdates;
}

int DataTracker::updateTouchedByCallee(FunctionDecl *Callee,
                                       const std::vector<uint8_t> &ParamModes,
                                       const boost::container::flat_set<ValueDecl *> &GlobalsAccessed,
                                       const std::vector<uint8_t> &GlobalModes) {
  int numUpdates = 0;

  // Start by finding all the calls to the callee.
  std::vector<CallExpr *> Calls;
  for (CallExpr *CE : CallExprs) {
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
    switch (Entry.Flags & A_OFFLD)
    {
    case A_OFFLD:
      llvm::outs() << "TGTDEV   ";
      break;
    default:
      llvm::outs() << "HOST     ";
      break;
    }
    llvm::outs() << Entry.Loc.printToString(SourceMgr) << "\n";
  }
  llvm::outs() << "\n";
  return;
}

int DataTracker::recordTargetRegion(TargetRegion *TR) {
  LastTargetRegion = TR;
  TargetRegions.push_back(TR);
  return 1;
}

int DataTracker::recordCallExpr(CallExpr *CE) {
  CallExprs.push_back(CE);
  return 1;
}

int DataTracker::recordLoop(Stmt *S) {
  // Don't record loops inside a target region
  if (LastTargetRegion != NULL && LastTargetRegion->contains(S->getBeginLoc()))
    return 0;

  Loops.push_back(S);

  AccessInfo NewEntry;
  NewEntry.VD    = nullptr;
  NewEntry.S     = S;
  NewEntry.Loc   = S->getBeginLoc();
  NewEntry.Flags = A_LBEGIN;
  insertAccessLogEntry(NewEntry);
  NewEntry.Loc   = S->getEndLoc();
  NewEntry.Flags = A_LEND;
  insertAccessLogEntry(NewEntry);
  return 1;
}

int DataTracker::recordLocal(ValueDecl *VD) {
  Locals.insert(VD);
  return 1;
}

int DataTracker::recordGlobal(ValueDecl *VD) {
  Globals.insert(VD);
  return 1;
}

const std::vector<TargetRegion *> &DataTracker::getTargetRegions() const {
  return TargetRegions;
}

const std::vector<CallExpr *> &DataTracker::getCallExprs() const {
  return CallExprs;
}

const std::vector<Stmt *> &DataTracker::getLoops() const {
  return Loops;
}

const TargetDataScope *DataTracker::getTargetDataScope() const {
  return TargetScope;
}

const boost::container::flat_set<ValueDecl *> &DataTracker::getLocals() const {
  return Locals;
}

const boost::container::flat_set<ValueDecl *> &DataTracker::getGlobals() const {
  return Globals;
}

// Finds the outermost Stmt in ContainingStmt that captures S. Returns NULL on
// error.
const Stmt *DataTracker::findOutermostCapturingStmt(Stmt *ContainingStmt, Stmt *S) {
  const Stmt *CurrentStmt = S;
  while (true) {
    const auto &ImmediateParents = Context->getParents(*CurrentStmt);
    if (ImmediateParents.size() == 0)
      return NULL;

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
  for (TargetRegion *TR : TargetRegions) {
    AccessInfo Lower, Upper;
    Lower.Loc = TR->getBeginLoc();
    Upper.Loc = TR->getEndLoc();
    TR->AccessLogBegin = std::lower_bound(AccessLog.begin(), AccessLog.end(), Lower);
    TR->AccessLogEnd   = std::upper_bound(AccessLog.begin(), AccessLog.end(), Upper);
  }

  for (TargetRegion *TR : TargetRegions) {
    // Seperate reads and writes into their own sets.
    TR->ReadDecls.clear();
    TR->WriteDecls.clear();
    for (auto It = TR->AccessLogBegin; It != TR->AccessLogEnd; ++It) {
      switch (It->Flags & 0b00000111)
      {
      case A_RDONLY:
        // Here we only want to map to if a variable was read before being
        // written to. This is more aggressive for arrays since the whole array
        // may not have been rewritten.
        if (!TR->WriteDecls.contains(It->VD))
          TR->ReadDecls.insert(It->VD);
        break;
      case A_WRONLY:
        TR->WriteDecls.insert(It->VD);
        break;
      case A_UNKNOWN:
      case A_RDWR:
        TR->ReadDecls.insert(It->VD);
        TR->WriteDecls.insert(It->VD);
        break;
      case A_NOP:
      default:
        break;
      }
    }

    std::set_difference(TR->ReadDecls.begin(), TR->ReadDecls.end(),
                        TR->WriteDecls.begin(), TR->WriteDecls.end(),
                        std::inserter(TR->MapTo, TR->MapTo.begin()));
    std::set_difference(TR->WriteDecls.begin(), TR->WriteDecls.end(),
                        TR->ReadDecls.begin(), TR->ReadDecls.end(),
                        std::inserter(TR->MapFrom, TR->MapFrom.begin()));
    std::set_intersection(TR->ReadDecls.begin(), TR->ReadDecls.end(),
                          TR->WriteDecls.begin(), TR->WriteDecls.end(),
                          std::inserter(TR->MapToFrom, TR->MapToFrom.begin()));
  }

  return;
}

struct DataFlowOf {
  ValueDecl *VD;
  DataFlowOf(ValueDecl *VD) : VD(VD) {}
  bool operator()(const AccessInfo &Entry) {
    // Consider an entry in the access log to be in the flow of VD if the entry
    // contains info for VD or is a the beginning or end of a loop.
    return (Entry.Flags & (A_LBEGIN | A_LEND))
           || (Entry.VD == VD);
  }
};

void DataTracker::analyzeValueDecl(ValueDecl *VD) {
  bool MapTo = false;
  bool MapFrom = false;
  bool DataInitialized = false;
  bool DataValidOnHost = false;
  bool DataValidOnDevice = false;
  std::stack<LoopDependency> LoopDependencyStack;

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
    if (It->Flags & A_LBEGIN) {
      LoopDependency LD;
      LD.DataValidOnHost   = DataValidOnHost;
      LD.DataValidOnDevice = DataValidOnDevice;
      LD.MapTo             = MapTo;
      LD.FirstHostAccess   = nullptr;
      LoopDependencyStack.emplace(LD);

    } else if (It->Flags & A_LEND) {
      LoopDependency LD = LoopDependencyStack.top();
      if (LD.DataValidOnHost && !DataValidOnHost && LD.FirstHostAccess) {
        AccessInfo FirstAccess = *(LD.FirstHostAccess); // copy
        FirstAccess.Flags |= A_LEND; // Indicates to the rewriter that this
                                     // statement is to be placed directive
                                     // directly before the loop end.
        TargetScope->UpdateFrom.emplace(FirstAccess);
      }
      if ((LD.DataValidOnDevice && !DataValidOnDevice && LD.FirstHostAccess)
       || (!LD.MapTo && MapTo && !DataValidOnDevice)) {
        TargetScope->UpdateTo.emplace(*PrevHostIt);
        MapTo = LD.MapTo; // restore MapTo to before loop
      }
      LoopDependencyStack.pop();

    } else if (It->Flags & A_OFFLD) {
      // check access on target device
      if (!DataInitialized) {
        if (It->Flags & A_RDONLY) {
          // read before write!
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
          TargetScope->UpdateTo.emplace(*PrevHostIt);
        }
        DataValidOnDevice = true;
      }
      if (It->Flags & (A_WRONLY | A_UNKNOWN)) {// Write/ReadWrite/Unknown
        DataValidOnDevice = true;
        DataValidOnHost = false;
      }

      // end check access on target device
    } else {
      // check access on host
      if (!DataInitialized) {
        if (It->Loc == It->VD->getLocation()
         && TargetScope->BeginLoc <= It->Loc) {
          // data needs to be declared before the target scope in which it is used.
          DiagnosticsEngine &DiagEngine = Context->getDiagnostics();
          const unsigned int DiagID = DiagEngine.getCustomDiagID(
              DiagnosticsEngine::Warning,
              "declaration of '%0' is captured within the target data region in which it is being utilized");
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
          TargetScope->UpdateFrom.emplace(*It);
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
    TargetScope->MapToFrom.insert(VD);
  else if (MapTo)
    TargetScope->MapTo.insert(VD);
  else if (MapFrom)
    TargetScope->MapFrom.insert(VD);
  else
    TargetScope->MapAlloc.insert(VD);

  return;
}

void DataTracker::analyze() {
  if (TargetRegions.size() == 0) {
    // There is nothing to analyze.
    return;
  }

  // Find target region bounds.
  for (TargetRegion *TR : TargetRegions) {
    AccessInfo Lower, Upper;
    Lower.Loc = TR->getBeginLoc();
    Upper.Loc = TR->getEndLoc();
    TR->AccessLogBegin = std::lower_bound(AccessLog.begin(), AccessLog.end(), Lower);
    TR->AccessLogEnd   = std::upper_bound(AccessLog.begin(), AccessLog.end(), Upper);
  }

  // Identify the root(outermost) target scope in the function.
  const Stmt *FrontCapturingStmt = findOutermostCapturingStmt(FD->getBody(), TargetRegions[0]->getDirective());
  SourceLocation ScopeBegin = FrontCapturingStmt->getBeginLoc();
  const Stmt *BackCapturingStmt  = findOutermostCapturingStmt(FD->getBody(), TargetRegions.back()->getDirective());
  SourceLocation ScopeEnd = BackCapturingStmt->getEndLoc();
  if (auto OpenMPDirective = dyn_cast<OMPExecutableDirective>(BackCapturingStmt)) {
    // ugh. The getEndLoc() of OpenMP directives is different and doesn't
    // consider the captured statement while seemingly every other statement
    // does.
    ScopeEnd = OpenMPDirective->getInnermostCapturedStmt()->getEndLoc();
  }

  TargetScope = new TargetDataScope(ScopeBegin, ScopeEnd, FD);

  // Map a list of all the data the TargetScope will be responsible for.
  boost::container::flat_set<ValueDecl *> TargetScopeDecls;
  for (auto It = AccessLog.begin(); It != AccessLog.end(); ++It) {
    if (It->Flags & A_OFFLD)
      TargetScopeDecls.insert(It->VD);
  }

  for (ValueDecl *VD : TargetScopeDecls) {
    analyzeValueDecl(VD);
  }

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

  for (ValueDecl *Global : Globals) {
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

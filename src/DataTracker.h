#ifndef DATATRACKER_H
#define DATATRACKER_H

#include <stack>

#include <boost/container/flat_set.hpp>

#include "TargetDataRegion.h"
#include "Kernel.h"

using namespace clang;

class DataTracker {
private:
  const FunctionDecl *FD;
  ASTContext *Context; 
  Kernel *LastKernel;
  TargetDataRegion *TargetScope;

  std::vector<AccessInfo> AccessLog;
  std::vector<Kernel *> Kernels;
  std::vector<const Stmt *> Loops;
  std::vector<const Stmt *> Conds;
  std::vector<const CallExpr *> CallExprs;
  boost::container::flat_set<const ValueDecl *> Locals;
  boost::container::flat_set<const ValueDecl *> Globals;
  boost::container::flat_set<int64_t> Disabled;

  const ValueDecl *LastArrayBasePointer;
  const ArraySubscriptExpr *LastArraySubscript;

  int recordGlobal(const ValueDecl *VD);
  int updateParamsTouchedByCallee(const FunctionDecl *Callee,
                                  const std::vector<const CallExpr *> &Calls,
                                  const std::vector<uint8_t> &ParamFlags);
  int updateGlobalsTouchedByCallee(const FunctionDecl *Callee,
                                   const std::vector<const CallExpr *> &Calls,
                                   const boost::container::flat_set<const ValueDecl *> &GlobalsAccessed,
                                   const std::vector<uint8_t> &GlobalFlags);
  const Stmt *findOutermostCapturingStmt(const Stmt *ContainingStmt,
                                         const Stmt *S) const;
  const AccessInfo *findOutermostIndexingLoop(std::vector<AccessInfo>::iterator &A,
                                              std::vector<const AccessInfo *> &LoopStack,
                                              std::vector<AccessInfo>::iterator &insertionLocLim) const;
  int insertAccessLogEntry(const AccessInfo &NewEntry);
  void analyzeValueDecl(const ValueDecl *VD);
  void analyzeValueDeclArrayBounds(const ValueDecl *VD);

public:
  DataTracker(FunctionDecl *FD, ASTContext *Context);

  const FunctionDecl *getDecl() const;

  bool contains(SourceLocation Loc) const;

  // Returns 1 if value was actually recorded, 0 otherwise.
  int recordAccess(const ValueDecl *VD, SourceLocation Loc, const Stmt *S, 
                   uint8_t Flags, bool overwrite = false);
  const std::vector<AccessInfo> &getAccessLog();
  // Returns int indicating number of updated log entries.
  int updateTouchedByCallee(const FunctionDecl *Callee,
                            const std::vector<uint8_t> &ParamFlags,
                            const boost::container::flat_set<const ValueDecl *> &GlobalsAccessed,
                            const std::vector<uint8_t> &GlobalFlags);
  void printAccessLog() const;

  int recordTargetRegion(Kernel *K);
  int recordCallExpr(const CallExpr *CE);
  int recordArrayAccess(const ValueDecl *BasePointer,
                        const ArraySubscriptExpr *Subscript);
  int recordLoop(const Stmt *S);
  int recordCond(const Stmt *S);
  int recordLocal(const ValueDecl *VD);

  const std::vector<Kernel *> &getTargetRegions() const;
  const std::vector<const CallExpr *> &getCallExprs() const;
  const std::vector<const Stmt *> &getLoops() const;
  const TargetDataRegion *getTargetDataScope() const;
  const boost::container::flat_set<const ValueDecl *> &getLocals() const;
  const boost::container::flat_set<const ValueDecl *> &getGlobals() const;

  void classifyOffloadedOps();
  void naiveAnalyze();
  void analyze();
  std::vector<uint8_t> getParamAccessModes(bool crossFnOffloading);
  std::vector<uint8_t> getGlobalAccessModes(bool crossFnOffloading);
};

#endif
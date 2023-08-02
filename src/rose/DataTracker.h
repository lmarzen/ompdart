#ifndef DATATRACKER_H
#define DATATRACKER_H

#include <stack>

#include <boost/container/flat_set.hpp>

#include "TargetDataScope.h"
#include "TargetRegion.h"

using namespace clang;

class DataTracker {
private:
  FunctionDecl *FD;
  ASTContext *Context; 
  TargetRegion *LastTargetRegion;
  TargetDataScope *TargetScope;

  std::vector<AccessInfo> AccessLog;
  std::vector<TargetRegion *> TargetRegions;
  std::vector<Stmt *> Loops;
  std::vector<CallExpr *> CallExprs;
  boost::container::flat_set<ValueDecl *> Locals;
  boost::container::flat_set<ValueDecl *> Globals;

  int recordGlobal(ValueDecl *VD);
  int updateParamsTouchedByCallee(FunctionDecl *Callee,
                                  const std::vector<CallExpr *> &Calls,
                                  const std::vector<uint8_t> &ParamFlags);
  int updateGlobalsTouchedByCallee(FunctionDecl *Callee,
                                   const std::vector<CallExpr *> &Calls,
                                   const boost::container::flat_set<ValueDecl *> &GlobalsAccessed,
                                   const std::vector<uint8_t> &GlobalFlags);
  const Stmt *findOutermostCapturingStmt(Stmt *ContainingStmt, Stmt *S);
      // TreeNode<Stmt *> *FindParentLoop(Stmt *S);
  std::vector<AccessInfo>::iterator find(
      std::vector<AccessInfo>::iterator First,
      std::vector<AccessInfo>::iterator Last,
      ValueDecl *Value,
      unsigned int ModeMask=0, unsigned int Flags=0);
  void findAccessLogEntry(std::vector<AccessInfo>::iterator &It,
                         std::vector<TargetRegion *>::iterator &TR, 
                         ValueDecl *TargetVD);
  int insertAccessLogEntry(const AccessInfo &NewEntry);
  void analyzeValueDecl(ValueDecl *VD);

public:
  DataTracker(FunctionDecl *FD, ASTContext *Context);

  FunctionDecl *getDecl() const;

  bool contains(SourceLocation Loc) const;

  // Returns 1 if value was actually recorded, 0 otherwise.
  int recordAccess(ValueDecl *VD, SourceLocation Loc, Stmt *S, 
                   uint8_t Flags, bool overwrite = false);
  const std::vector<AccessInfo> &getAccessLog();
  // Returns int indicating number of updated log entries.
  int updateTouchedByCallee(FunctionDecl *Callee,
                            const std::vector<uint8_t> &ParamFlags,
                            const boost::container::flat_set<ValueDecl *> &GlobalsAccessed,
                            const std::vector<uint8_t> &GlobalFlags);
  void printAccessLog() const;

  int recordTargetRegion(TargetRegion *TR);
  int recordCallExpr(CallExpr *CE);
  int recordLoop(Stmt *S);
  int recordLocal(ValueDecl *VD);

  const std::vector<TargetRegion *> &getTargetRegions() const;
  const std::vector<CallExpr *> &getCallExprs() const;
  const std::vector<Stmt *> &getLoops() const;
  const TargetDataScope *getTargetDataScope() const;
  const boost::container::flat_set<ValueDecl *> &getLocals() const;
  const boost::container::flat_set<ValueDecl *> &getGlobals() const;

  void naiveAnalyze();
  void analyze();
  std::vector<uint8_t> getParamAccessModes() const;
  std::vector<uint8_t> getGlobalAccessModes() const;
};

#endif
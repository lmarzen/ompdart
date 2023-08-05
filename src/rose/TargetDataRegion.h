#ifndef TARGETDATAREGION_H
#define TARGETDATAREGION_H

#include <vector>

#include "AccessInfo.h"
#include "ClauseInfo.h"

using namespace clang;

class TargetDataRegion {
private:
  SourceLocation BeginLoc;
  SourceLocation EndLoc;
  FunctionDecl *FD;

  std::vector<ValueDecl *> MapTo;
  std::vector<ValueDecl *> MapFrom;
  std::vector<ValueDecl *> MapToFrom;
  std::vector<ValueDecl *> MapAlloc;
  std::vector<AccessInfo> UpdateTo;
  std::vector<AccessInfo> UpdateFrom;
  std::vector<ClauseInfo> Private;
  std::vector<ClauseInfo> FirstPrivate;

  // will directly update
  friend class DataTracker;

public:
  TargetDataRegion(SourceLocation BeginLoc, SourceLocation EndLoc, FunctionDecl *FD);

  SourceLocation getBeginLoc() const;
  SourceLocation getEndLoc() const;
  FunctionDecl *getContainingFunction() const;
  void print(llvm::raw_ostream &OS, const SourceManager &SM) const;

  const std::vector<ValueDecl *> &getMapTo() const;
  const std::vector<ValueDecl *> &getMapFrom() const;
  const std::vector<ValueDecl *> &getMapToFrom() const;
  const std::vector<ValueDecl *> &getMapAlloc() const;
  const std::vector<AccessInfo> &getUpdateTo() const;
  const std::vector<AccessInfo> &getUpdateFrom() const;
  const std::vector<ClauseInfo> &getPrivate() const;
  const std::vector<ClauseInfo> &getFirstPrivate() const;
};

#endif
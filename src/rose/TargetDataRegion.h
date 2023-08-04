#ifndef TARGETDATAREGION_H
#define TARGETDATAREGION_H

#include <boost/container/flat_set.hpp>

#include "AccessInfo.h"

using namespace clang;

class TargetDataRegion {
private:
  SourceLocation BeginLoc;
  SourceLocation EndLoc;
  FunctionDecl *FD;

  boost::container::flat_set<ValueDecl *> MapTo;
  boost::container::flat_set<ValueDecl *> MapFrom;
  boost::container::flat_set<ValueDecl *> MapToFrom;
  boost::container::flat_set<ValueDecl *> MapAlloc;
  boost::container::flat_set<AccessInfo> UpdateTo;
  boost::container::flat_set<AccessInfo> UpdateFrom;

  // will directly update
  friend class DataTracker;

public:
  TargetDataRegion(SourceLocation BeginLoc, SourceLocation EndLoc, FunctionDecl *FD);

  SourceLocation getBeginLoc() const;
  SourceLocation getEndLoc() const;
  FunctionDecl *getContainingFunction() const;
  void print(llvm::raw_ostream &OS, const SourceManager &SM) const;

  const boost::container::flat_set<ValueDecl *> &getMapTo() const;
  const boost::container::flat_set<ValueDecl *> &getMapFrom() const;
  const boost::container::flat_set<ValueDecl *> &getMapToFrom() const;
  const boost::container::flat_set<ValueDecl *> &getMapAlloc() const;
  const boost::container::flat_set<AccessInfo> &getUpdateTo() const;
  const boost::container::flat_set<AccessInfo> &getUpdateFrom() const;
};

#endif
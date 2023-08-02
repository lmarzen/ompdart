#ifndef TARGETREGION_H
#define TARGETREGION_H

#include <boost/container/flat_set.hpp>

#include "clang/AST/StmtOpenMP.h"

#include "AccessInfo.h"

using namespace clang;

class TargetRegion {
private:
  OMPExecutableDirective *TD;
  FunctionDecl *FD;

  boost::container::flat_set<ValueDecl *> PrivateDecls;
  boost::container::flat_set<ValueDecl *> MapTo;
  boost::container::flat_set<ValueDecl *> MapFrom;
  boost::container::flat_set<ValueDecl *> MapToFrom;
  boost::container::flat_set<ValueDecl *> MapAlloc;
  boost::container::flat_set<ValueDecl *> ReadDecls;
  boost::container::flat_set<ValueDecl *> WriteDecls;
  // These variables are already present in the device's data environment,
  // indicating that no mapping is necessary.
  boost::container::flat_set<ValueDecl *> PreExistingData;

  // will update maps
  friend class DataTracker;
  std::vector<AccessInfo>::iterator AccessLogBegin;
  std::vector<AccessInfo>::iterator AccessLogEnd;

public:
  TargetRegion(OMPExecutableDirective *TD, FunctionDecl *FD);

  OMPExecutableDirective *getDirective() const;
  FunctionDecl *getFunction() const;

  bool contains(SourceLocation Loc) const;
  SourceLocation getBeginLoc() const;
  SourceLocation getEndLoc() const;

  int recordPrivate(ValueDecl *VD);
  const boost::container::flat_set<ValueDecl *> &getPrivateDecls() const;
  bool isPrivate(ValueDecl *VD) const;
  void print(llvm::raw_ostream &OS, const SourceManager &SM) const;
  
  const boost::container::flat_set<ValueDecl *> &getMapTo() const;
  const boost::container::flat_set<ValueDecl *> &getMapFrom() const;
  const boost::container::flat_set<ValueDecl *> &getMapToFrom() const;
  const boost::container::flat_set<ValueDecl *> &getMapAlloc() const;
};

#endif
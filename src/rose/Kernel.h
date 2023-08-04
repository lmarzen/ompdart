#ifndef KERNEL_H
#define KERNEL_H

#include <boost/container/flat_set.hpp>

#include "clang/AST/StmtOpenMP.h"

#include "AccessInfo.h"

using namespace clang;

class Kernel {
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

  std::vector<AccessInfo>::iterator AccessLogBegin;
  std::vector<AccessInfo>::iterator AccessLogEnd;

  std::vector<OMPExecutableDirective *> NestedDirectives;

  friend class DataTracker; // will update this class directly

public:
  Kernel(OMPExecutableDirective *TD, FunctionDecl *FD);

  OMPExecutableDirective *getDirective() const;
  FunctionDecl *getFunction() const;

  bool contains(SourceLocation Loc) const;
  SourceLocation getBeginLoc() const;
  SourceLocation getEndLoc() const;

  int recordPrivate(ValueDecl *VD);
  const boost::container::flat_set<ValueDecl *> &getPrivateDecls() const;
  bool isPrivate(ValueDecl *VD) const;
  int recordNestedDirective(OMPExecutableDirective *TD);
  void print(llvm::raw_ostream &OS, const SourceManager &SM) const;
  
  const boost::container::flat_set<ValueDecl *> &getMapTo() const;
  const boost::container::flat_set<ValueDecl *> &getMapFrom() const;
  const boost::container::flat_set<ValueDecl *> &getMapToFrom() const;
  const boost::container::flat_set<ValueDecl *> &getMapAlloc() const;
};

#endif
#ifndef KERNEL_H
#define KERNEL_H

#include <boost/container/flat_set.hpp>

#include "clang/AST/StmtOpenMP.h"

#include "AccessInfo.h"

using namespace clang;

class Kernel {
private:
  ASTContext *Context; 
  const OMPExecutableDirective *TD;
  const FunctionDecl *FD;

  boost::container::flat_set<const ValueDecl *> PrivateDecls;
  boost::container::flat_set<const ValueDecl *> MapTo;
  boost::container::flat_set<const ValueDecl *> MapFrom;
  boost::container::flat_set<const ValueDecl *> MapToFrom;
  boost::container::flat_set<const ValueDecl *> MapAlloc;
  boost::container::flat_set<const ValueDecl *> ReadDecls;
  boost::container::flat_set<const ValueDecl *> WriteDecls;

  std::vector<AccessInfo>::iterator AccessLogBegin;
  std::vector<AccessInfo>::iterator AccessLogEnd;

  std::vector<const OMPExecutableDirective *> NestedDirectives;

  friend class DataTracker; // will update this class directly

public:
  Kernel(const OMPExecutableDirective *TD, const FunctionDecl *FD,
         ASTContext *Context);

  const OMPExecutableDirective *getDirective() const;
  const FunctionDecl *getFunction() const;

  bool contains(SourceLocation Loc) const;
  SourceLocation getBeginLoc() const;
  SourceLocation getEndLoc() const;

  int recordPrivate(const ValueDecl *VD);
  const boost::container::flat_set<const ValueDecl *> &getPrivateDecls() const;
  bool isPrivate(const ValueDecl *VD) const;
  int recordNestedDirective(const OMPExecutableDirective *TD);
  void print(llvm::raw_ostream &OS, const SourceManager &SM) const;
  
  const boost::container::flat_set<const ValueDecl *> &getMapTo() const;
  const boost::container::flat_set<const ValueDecl *> &getMapFrom() const;
  const boost::container::flat_set<const ValueDecl *> &getMapToFrom() const;
  const boost::container::flat_set<const ValueDecl *> &getMapAlloc() const;
};

#endif
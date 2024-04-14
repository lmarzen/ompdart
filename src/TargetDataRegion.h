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
  const FunctionDecl *FD;

  std::vector<AccessInfo> MapTo;
  std::vector<AccessInfo> MapFrom;
  std::vector<AccessInfo> MapToFrom;
  std::vector<AccessInfo> MapAlloc;
  std::vector<AccessInfo> UpdateTo;
  std::vector<AccessInfo> UpdateFrom;
  std::vector<ClauseInfo> Private;
  std::vector<ClauseInfo> FirstPrivate;
  std::vector<const OMPExecutableDirective *> Kernels;

  // will directly update
  friend class DataTracker;

public:
  TargetDataRegion(SourceLocation BeginLoc, SourceLocation EndLoc,
                   const FunctionDecl *FD);

  SourceLocation getBeginLoc() const;
  SourceLocation getEndLoc() const;
  const FunctionDecl *getContainingFunction() const;
  void print(llvm::raw_ostream &OS, const SourceManager &SM) const;

  const std::vector<AccessInfo> &getMapTo() const;
  const std::vector<AccessInfo> &getMapFrom() const;
  const std::vector<AccessInfo> &getMapToFrom() const;
  const std::vector<AccessInfo> &getMapAlloc() const;
  const std::vector<AccessInfo> &getUpdateTo() const;
  const std::vector<AccessInfo> &getUpdateFrom() const;
  const std::vector<ClauseInfo> &getPrivate() const;
  const std::vector<ClauseInfo> &getFirstPrivate() const;
  const std::vector<const OMPExecutableDirective *> &getKernels() const;
};

#endif

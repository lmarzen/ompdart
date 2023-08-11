#include "TargetDataRegion.h"

using namespace clang;

TargetDataRegion::TargetDataRegion(SourceLocation BeginLoc, SourceLocation EndLoc,
                                   const FunctionDecl *FD)
    : BeginLoc(BeginLoc), EndLoc(EndLoc), FD(FD) {}

SourceLocation TargetDataRegion::getBeginLoc() const {
  return BeginLoc;
}

SourceLocation TargetDataRegion::getEndLoc() const {
  return EndLoc;
}

const FunctionDecl *TargetDataRegion::getContainingFunction() const {
  return FD;
}

void TargetDataRegion::print(llvm::raw_ostream &OS, const SourceManager &SM) const {
  llvm::outs() << "\n|-- Location: ";
  BeginLoc.print(llvm::outs(), SM);
  llvm::outs() << "\n|             ";
  EndLoc.print(llvm::outs(), SM);

  llvm::outs() << "\n|-- Data";
  if (MapTo.size())
    llvm::outs() << "\n|   |-- to";
  for (const ValueDecl *VD : MapTo) {
    llvm::outs() << "\n|   |   |-- " << VD->getNameAsString() << " loc: ";
    VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << VD->getID();
  }
  if (MapFrom.size())
    llvm::outs() << "\n|   |-- from";
  for (const ValueDecl *VD : MapFrom) {
    llvm::outs() << "\n|   |   |-- " << VD->getNameAsString() << " loc: ";
    VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << VD->getID();
  }
  if (MapToFrom.size())
    llvm::outs() << "\n|   |-- tofrom";
  for (const ValueDecl *VD : MapToFrom) {
    llvm::outs() << "\n|   |   |-- " << VD->getNameAsString() << " loc: ";
    VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << VD->getID();
  }
  if (MapAlloc.size())
    llvm::outs() << "\n|   |-- alloc";
  for (const ValueDecl *VD : MapAlloc) {
    llvm::outs() << "\n|   |   |-- " << VD->getNameAsString() << " loc: ";
    VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << VD->getID();
  }
  if (UpdateFrom.size())
    llvm::outs() << "\n|   |-- updatefrom";
  for (AccessInfo Access : UpdateFrom) {
    llvm::outs() << "\n|   |   |-- " << Access.VD->getNameAsString() << " loc: ";
    Access.Loc.print(llvm::outs(), SM);
    llvm::outs() << " id: " << Access.VD->getID();
  }
  if (UpdateTo.size())
    llvm::outs() << "\n|   |-- updateto";
  for (AccessInfo Access : UpdateTo) {
    llvm::outs() << "\n|   |   |-- " << Access.VD->getNameAsString() << " loc: ";
    Access.Loc.print(llvm::outs(), SM);
    llvm::outs() << " id: " << Access.VD->getID();
  }
  llvm::outs() << "\n";

  return;
}

const std::vector<const ValueDecl *> &TargetDataRegion::getMapTo() const {
  return MapTo;
}

const std::vector<const ValueDecl *> &TargetDataRegion::getMapFrom() const {
  return MapFrom;
}

const std::vector<const ValueDecl *> &TargetDataRegion::getMapToFrom() const {
  return MapToFrom;
}

const std::vector<const ValueDecl *> &TargetDataRegion::getMapAlloc() const {
  return MapAlloc;
}

const std::vector<AccessInfo> &TargetDataRegion::getUpdateTo() const {
  return UpdateTo;
}

const std::vector<AccessInfo> &TargetDataRegion::getUpdateFrom() const {
  return UpdateFrom;
}

const std::vector<ClauseInfo> &TargetDataRegion::getPrivate() const {
  return Private;
}

const std::vector<ClauseInfo> &TargetDataRegion::getFirstPrivate() const {
  return FirstPrivate;
}

const std::vector<const OMPExecutableDirective *> &TargetDataRegion::getKernels() const {
  return Kernels;
}
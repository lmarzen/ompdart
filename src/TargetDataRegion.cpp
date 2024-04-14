#include "TargetDataRegion.h"

using namespace clang;

TargetDataRegion::TargetDataRegion(SourceLocation BeginLoc,
                                   SourceLocation EndLoc,
                                   const FunctionDecl *FD)
    : BeginLoc(BeginLoc), EndLoc(EndLoc), FD(FD) {}

SourceLocation TargetDataRegion::getBeginLoc() const { return BeginLoc; }

SourceLocation TargetDataRegion::getEndLoc() const { return EndLoc; }

const FunctionDecl *TargetDataRegion::getContainingFunction() const {
  return FD;
}

void TargetDataRegion::print(llvm::raw_ostream &OS,
                             const SourceManager &SM) const {
  llvm::outs() << "\n|-- Location: ";
  BeginLoc.print(llvm::outs(), SM);
  llvm::outs() << "\n|             ";
  EndLoc.print(llvm::outs(), SM);

  llvm::outs() << "\n|-- Data";
  if (MapTo.size())
    llvm::outs() << "\n|   |-- to";
  for (const AccessInfo &Access : MapTo) {
    llvm::outs() << "\n|   |   |-- " << Access.VD->getNameAsString()
                 << " loc: ";
    Access.VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << Access.VD->getID();
  }
  if (MapFrom.size())
    llvm::outs() << "\n|   |-- from";
  for (const AccessInfo &Access : MapFrom) {
    llvm::outs() << "\n|   |   |-- " << Access.VD->getNameAsString()
                 << " loc: ";
    Access.VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << Access.VD->getID();
  }
  if (MapToFrom.size())
    llvm::outs() << "\n|   |-- tofrom";
  for (const AccessInfo &Access : MapToFrom) {
    llvm::outs() << "\n|   |   |-- " << Access.VD->getNameAsString()
                 << " loc: ";
    Access.VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << Access.VD->getID();
  }
  if (MapAlloc.size())
    llvm::outs() << "\n|   |-- alloc";
  for (const AccessInfo &Access : MapAlloc) {
    llvm::outs() << "\n|   |   |-- " << Access.VD->getNameAsString()
                 << " loc: ";
    Access.VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << Access.VD->getID();
  }
  if (UpdateFrom.size())
    llvm::outs() << "\n|   |-- updatefrom";
  for (const AccessInfo &Access : UpdateFrom) {
    llvm::outs() << "\n|   |   |-- " << Access.VD->getNameAsString()
                 << " loc: ";
    Access.Loc.print(llvm::outs(), SM);
    llvm::outs() << " id: " << Access.VD->getID();
  }
  if (UpdateTo.size())
    llvm::outs() << "\n|   |-- updateto";
  for (const AccessInfo &Access : UpdateTo) {
    llvm::outs() << "\n|   |   |-- " << Access.VD->getNameAsString()
                 << " loc: ";
    Access.Loc.print(llvm::outs(), SM);
    llvm::outs() << " id: " << Access.VD->getID();
  }
  llvm::outs() << "\n";

  return;
}

const std::vector<AccessInfo> &TargetDataRegion::getMapTo() const {
  return MapTo;
}

const std::vector<AccessInfo> &TargetDataRegion::getMapFrom() const {
  return MapFrom;
}

const std::vector<AccessInfo> &TargetDataRegion::getMapToFrom() const {
  return MapToFrom;
}

const std::vector<AccessInfo> &TargetDataRegion::getMapAlloc() const {
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

const std::vector<const OMPExecutableDirective *> &
TargetDataRegion::getKernels() const {
  return Kernels;
}

#include "TargetDataScope.h"

using namespace clang;

TargetDataScope::TargetDataScope(SourceLocation BeginLoc, SourceLocation EndLoc,
                                 FunctionDecl *FD)
    : BeginLoc(BeginLoc), EndLoc(EndLoc), FD(FD) {}

SourceLocation TargetDataScope::getBeginLoc() const {
  return BeginLoc;
}

SourceLocation TargetDataScope::getEndLoc() const {
  return EndLoc;
}

FunctionDecl *TargetDataScope::getContainingFunction() const {
  return FD;
}

void TargetDataScope::print(llvm::raw_ostream &OS, const SourceManager &SM) const {
  llvm::outs() << "\n|-- Location: ";
  BeginLoc.print(llvm::outs(), SM);
  llvm::outs() << "\n|             ";
  EndLoc.print(llvm::outs(), SM);

  llvm::outs() << "\n|-- Data";
  if (MapTo.size())
    llvm::outs() << "\n|   |-- to";
  for (ValueDecl *VD : MapTo) {
    llvm::outs() << "\n|   |   |-- " << VD->getNameAsString() << " loc: ";
    VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << VD->getID();
  }
  if (MapFrom.size())
    llvm::outs() << "\n|   |-- from";
  for (ValueDecl *VD : MapFrom) {
    llvm::outs() << "\n|   |   |-- " << VD->getNameAsString() << " loc: ";
    VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << VD->getID();
  }
  if (MapToFrom.size())
    llvm::outs() << "\n|   |-- tofrom";
  for (ValueDecl *VD : MapToFrom) {
    llvm::outs() << "\n|   |   |-- " << VD->getNameAsString() << " loc: ";
    VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << VD->getID();
  }
  if (MapAlloc.size())
    llvm::outs() << "\n|   |-- alloc";
  for (ValueDecl *VD : MapAlloc) {
    llvm::outs() << "\n|   |   |-- " << VD->getNameAsString() << " loc: ";
    VD->getLocation().print(llvm::outs(), SM);
    llvm::outs() << " " << VD->getID();
  }
  if (UpdateFrom.size())
    llvm::outs() << "\n|   |-- updatefrom";
  for (const AccessInfo Access : UpdateFrom) {
    llvm::outs() << "\n|   |   |-- " << Access.VD->getNameAsString() << " loc: ";
    Access.Loc.print(llvm::outs(), SM);
    llvm::outs() << " id: " << Access.VD->getID();
  }
  if (UpdateTo.size())
    llvm::outs() << "\n|   |-- updateto";
  for (const AccessInfo Access : UpdateTo) {
    llvm::outs() << "\n|   |   |-- " << Access.VD->getNameAsString() << " loc: ";
    Access.Loc.print(llvm::outs(), SM);
    llvm::outs() << " id: " << Access.VD->getID();
  }
  llvm::outs() << "\n";

  return;
}

const boost::container::flat_set<ValueDecl *> &TargetDataScope::getMapTo() const {
  return MapTo;
}

const boost::container::flat_set<ValueDecl *> &TargetDataScope::getMapFrom() const {
  return MapFrom;
}

const boost::container::flat_set<ValueDecl *> &TargetDataScope::getMapToFrom() const {
  return MapToFrom;
}

const boost::container::flat_set<ValueDecl *> &TargetDataScope::getMapAlloc() const {
  return MapAlloc;
}

const boost::container::flat_set<AccessInfo> &TargetDataScope::getUpdateTo() const {
  return UpdateTo;
}

const boost::container::flat_set<AccessInfo> &TargetDataScope::getUpdateFrom() const {
  return UpdateFrom;
}

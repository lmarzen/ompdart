#include "AnalysisUtils.h"

using namespace clang;

void performInterproceduralAnalysis(
    std::vector<DataTracker *> &FunctionTrackers) {
  // Using the information we have collected about read and writes we can
  // update calls to functions with the details about how a pointer was used
  // after it was passed. We may need to do this multiple times as we may need
  // to resolve more information about one function before we can update
  // another. So we loop until the number of unknown parameters converges.
  // TODO: This iterative approach is simplier than a recursive algorithm, but
  //       inherently cannot resolve recursions.
  int numUpdates;
  do {
    numUpdates = 0;
    for (DataTracker *DT : FunctionTrackers) {
      for (DataTracker *TmpDT : FunctionTrackers) {
        numUpdates += TmpDT->updateTouchedByCallee(
            DT->getDecl(), DT->getParamAccessModes(false), DT->getGlobals(),
            DT->getGlobalAccessModes(false));
      }
    }
#if DEBUG_LEVEL >= 1
    llvm::outs() << numUpdates << "\n";
#endif
  } while (numUpdates > 0);
  return;
}

void performAggressiveCrossFunctionOffloading(
    std::vector<DataTracker *> &FunctionTrackers) {
  // If the variable is accessed only on the target device, leave it to the
  // calling function to do the data mapping, otherwise we should clear the
  // A_OFFLD bit to indicate that this function expects move this parameter.
  // This is a little over generalized sense there may be an optimization
  // opportunity even if the function moves the data back and forth. I've
  // ignored this for now for simplicity and since applications are generally
  // not written that way.
  std::vector<std::vector<uint8_t>> ParamAccessModess;
  std::vector<std::vector<uint8_t>> GlobalAccessModess;
  for (DataTracker *DT : FunctionTrackers) {
    ParamAccessModess.emplace_back(DT->getParamAccessModes(true));
    GlobalAccessModess.emplace_back(DT->getGlobalAccessModes(true));
  }

  for (DataTracker *DT : FunctionTrackers) {
    for (int I = 0; I < FunctionTrackers.size(); ++I) {
#if DEBUG_LEVEL >= 1
      llvm::outs() << "ParamAccessModess for "
                   << DT->getDecl()->getNameAsString() << " calling "
                   << FunctionTrackers[I]->getDecl()->getNameAsString() << "\n";
#endif
      DT->updateTouchedByCallee(
          FunctionTrackers[I]->getDecl(), ParamAccessModess[I],
          FunctionTrackers[I]->getGlobals(), GlobalAccessModess[I]);
    }
  }
  return;
}

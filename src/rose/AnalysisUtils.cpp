#include "AnalysisUtils.h"

using namespace clang;

void performInterproceduralAnalysis(std::vector<DataTracker *> &FunctionTrackers) {
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
                        DT->getDecl(),    DT->getParamAccessModes(),
                        DT->getGlobals(), DT->getGlobalAccessModes());
      }
    }
    llvm::outs() << numUpdates << "\n";
  } while (numUpdates > 0);
  return;
}
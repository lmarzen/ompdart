#ifndef ANALYSISUTILS_H
#define ANALYSISUTILS_H

#include "DataTracker.h"

using namespace clang;

void performInterproceduralAnalysis(std::vector<DataTracker *> &FunctionTrackers);
void performAggressiveCrossFunctionOffloading(std::vector<DataTracker *> &FunctionTrackers);

#endif
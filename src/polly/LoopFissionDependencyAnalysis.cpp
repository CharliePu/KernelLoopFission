#include "polly/LoopFissionDependencyAnalysis.h"

#include "llvm/Support/Debug.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/IR/Instructions.h"
#include "polly/ScopInfo.h"
#include "polly/ScopDetection.h"
#include "polly/Support/ScopHelper.h"

#define DEBUG_TYPE "polly-loop-fission-dependency"

using namespace llvm;
using namespace polly;

AnalysisKey LoopFissionDependencyAnalysis::Key;

LoopFissionDependencyAnalysis::Result
LoopFissionDependencyAnalysis::run(Loop &L, LoopAnalysisManager &LAM,
                                    LoopStandardAnalysisResults &AR)
{
    Result result;
    result.loop = &L;
    result.validLoop = false;

    dbgs() << "Polly Loop Analysis view: " << "\n";
    for (BasicBlock *BB : L.blocks())
    {
        dbgs() << *BB << "\n";
    }
    dbgs() << "\n";

    // For the POC, we'll consider all loops in a Scop as valid for potential fission
    result.validLoop = true;
    
    return result;
}
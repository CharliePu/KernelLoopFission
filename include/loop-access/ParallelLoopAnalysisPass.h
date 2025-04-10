#ifndef PARALLEL_LOOP_ANALYSIS_PASS_H
#define PARALLEL_LOOP_ANALYSIS_PASS_H

#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Analysis/LoopAccessAnalysis.h"

#include <map>

namespace llvm {

// This is now a function-level analysis instead of a loop-level analysis
class ParallelLoopAnalysis : public AnalysisInfoMixin<ParallelLoopAnalysis>
{
public:
    // Result class stores info about all loops in a function
    class Result {
    public:
        // Maps loop header names to their parallel status
        std::map<std::string, bool> LoopParallelStatus;
        
        // Check if a specific loop is parallel
        bool isParallel(const Loop *L) const {
            auto it = LoopParallelStatus.find(L->getHeader()->getName().str());
            return it != LoopParallelStatus.end() ? it->second : false;
        }
    };
    
    // Run on function instead of loop
    Result run(Function &F, FunctionAnalysisManager &FAM);

private:
    friend AnalysisInfoMixin<ParallelLoopAnalysis>;
    static AnalysisKey Key;
};

// This is now a function pass that prints the parallel loop analysis results
class ParallelLoopAnalysisPrinterPass : public PassInfoMixin<ParallelLoopAnalysisPrinterPass>
{
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

}

#endif
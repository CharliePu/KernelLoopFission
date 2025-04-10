#ifndef LLVM_POLLY_LOOP_FISSION_DEPENDENCY_ANALYSIS_H
#define LLVM_POLLY_LOOP_FISSION_DEPENDENCY_ANALYSIS_H

#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "polly/ScopInfo.h"

namespace llvm
{

    class LoopFissionDependencyAnalysis : public AnalysisInfoMixin<LoopFissionDependencyAnalysis>
    {
    public:
        struct Result
        {
            Loop *loop;
            bool validLoop;
        };
        Result run(Loop &L, LoopAnalysisManager &LAM,
                    LoopStandardAnalysisResults &AR);

    private:
        friend AnalysisInfoMixin<LoopFissionDependencyAnalysis>;
        static AnalysisKey Key;
    };

} // namespace llvm

#endif // LLVM_POLLY_LOOP_FISSION_DEPENDENCY_ANALYSIS_H
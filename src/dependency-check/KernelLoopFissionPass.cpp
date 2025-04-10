#include "dependency-check/KernelLoopFissionPass.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

#include "dependency-check/LoopFissionDependencyAnalysis.h"




using namespace llvm;

PreservedAnalyses llvm::KernelLoopFissionPass::run(Function &F, FunctionAnalysisManager &FAM)
{
    dbgs() << "Running KernelLoopFissionPass on function: " << F.getName() << "\n";

    auto &LAMFP = FAM.getResult<LoopAnalysisManagerFunctionProxy>(F);
    auto &LAM = LAMFP.getManager();

    auto &AA = FAM.getResult<AAManager>(F);
    auto &AC = FAM.getResult<AssumptionAnalysis>(F);
    auto &DT = FAM.getResult<DominatorTreeAnalysis>(F);
    auto &LI = FAM.getResult<LoopAnalysis>(F);
    auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
    auto &TLI = FAM.getResult<TargetLibraryAnalysis>(F);
    auto &TTI = FAM.getResult<TargetIRAnalysis>(F);

    LoopStandardAnalysisResults AR = {AA, AC, DT, LI, SE, TLI, TTI};

    bool found = false;
    
    for (auto &loop : LI)
    {
        if (loop->isOutermost()) {
            auto result = LAM.getResult<LoopFissionDependencyAnalysis>(*loop, AR);
            if (result.validLoop)
            {
                found = true;
                break;
            }
        }
    }
    

    dbgs()<<"\n";
    dbgs()<<
        "=========================================================\n";
    if (found)
    {
        dbgs() << "Done. Found a valid loop for fission.\n";
    }
    else
    {
        dbgs() << "Done. No valid loop found for fission.\n";
    }

    return PreservedAnalyses();
}
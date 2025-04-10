#include "loop-access/ParallelLoopAnalysisPass.h"
#include "llvm/Analysis/LoopAccessAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

AnalysisKey ParallelLoopAnalysis::Key;

ParallelLoopAnalysis::Result ParallelLoopAnalysis::run(
    Function &F, FunctionAnalysisManager &FAM) {
    
    Result R;
    
    // Get the required analyses
    auto &LI = FAM.getResult<LoopAnalysis>(F);
    auto &LAA = FAM.getResult<LoopAccessAnalysis>(F);
    
    for (Loop *L : LI) {
        bool isParallel = true;
        
        const auto &LAI = LAA.getInfo(*L);
        const auto &depChecker = LAI.getDepChecker();
        
        auto deps = depChecker.getDependences();
        for (auto it = deps->begin(); it != deps->end(); ++it) {
            using llvm::MemoryDepChecker;
            auto &dep = *it;
            
            if (dep.Type != dep.NoDep) {
                isParallel = false;
                break;
            }
        }

        for (auto [instruction, order] : depChecker.generateInstructionOrderMap()) {
            dbgs() << "Instruction: " << *instruction << "\n";
            dbgs() << "Order: " << order << "\n";
        }

        dbgs() << "Number of dependencies found: " << deps->size() << "\n";
        for (auto it = deps->begin(); it != deps->end(); ++it) {
            auto &dep = *it;
            dbgs() << "  Dependency type: " << dep.Type << "\n";
        }

        dbgs() << "canVectorizeMemory:"<< LAI.canVectorizeMemory() << "\n";
        dbgs()<<"pointer checks"<<LAI.getNumRuntimePointerChecks() << "\n";
        dbgs()<<"num stores: "<<LAI.getNumStores() << "\n";
        dbgs()<<"num loads: "<<LAI.getNumLoads() << "\n";

        dbgs()<<"safe vector width"<<depChecker.getMaxSafeVectorWidthInBits() << "\n";
        dbgs()<<"is safe vectorize"<<depChecker.isSafeForVectorization() << "\n";

        

        
        R.LoopParallelStatus[L->getHeader()->getName().str()] = isParallel;
    }
    
    return R;
}

PreservedAnalyses ParallelLoopAnalysisPrinterPass::run(
    Function &F, FunctionAnalysisManager &FAM) {
    
    auto &result = FAM.getResult<ParallelLoopAnalysis>(F);
    auto &LI = FAM.getResult<LoopAnalysis>(F);
    
    for (Loop *L : LI) {

        if (result.isParallel(L)) {
            dbgs() << "\tParallel loop: " + L->getHeader()->getName() << "\n";
        } else {
            dbgs() << "\tNon-parallel loop: " + L->getHeader()->getName() << "\n";
        }
    }
    
    return PreservedAnalyses::all();
}
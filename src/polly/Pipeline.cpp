#include "polly/Pipeline.h"

#include "polly/LoopFissionDependencyAnalysis.h"
#include "polly/KernelLoopFissionPass.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Utils/LoopSimplify.h"
#include "llvm/Transforms/Scalar/IndVarSimplify.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Scalar/LICM.h"

#include "polly/RegisterPasses.h"
#include "polly/ScopDetection.h"
#include "polly/ScopInfo.h"
#include "polly/Simplify.h"

using namespace llvm;

MyPipeline buildPipeline() {
    MyPipeline pipeline;

    auto &PB = pipeline.PB;
    auto &LAM = pipeline.LAM;
    auto &FAM = pipeline.FAM;
    auto &CGAM = pipeline.CGAM;
    auto &MAM = pipeline.MAM;
    auto &FPM = pipeline.FPM;
    auto &LPM = pipeline.LPM;
    auto &MPM = pipeline.MPM;

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    FAM.registerPass([&]{ return polly::ScopAnalysis(); });
    FAM.registerPass([&]{ return polly::ScopInfoAnalysis(); });
    
    LAM.registerPass([&]{ return LoopFissionDependencyAnalysis(); });
        
    FPM.addPass(PromotePass());

    LPM.addPass(IndVarSimplifyPass());
    FPM.addPass(createFunctionToLoopPassAdaptor(std::move(LPM)));

    FPM.addPass(LCSSAPass());
    FPM.addPass(LoopSimplifyPass());
    
    MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));

    return pipeline;
}
#include "polly/DeviceKernelLoopFissionPass.h"
#include "common/helper.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/IRBuilder.h"

#define DEBUG_TYPE "device-kernel-loop-fission"

using namespace llvm;

PreservedAnalyses DeviceKernelLoopFissionPass::run(Module &M, ModuleAnalysisManager &MAM) {
    dbgs() << "Running DeviceKernelLoopFissionPass\n";
    
    FunctionAnalysisManager &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    
    for (Function &F : M) {
        auto funcName = F.getName().str();
        
        if (isCudaKernelDeviceStub(&F)) {
            dbgs()<<"Error : Function " << funcName << " is a device stub function\n";
            continue;
        }
        
        if (!isCudaKernel(&F))
            continue;
            
        dbgs() << "Transforming device kernel: " << F.getName() << "\n";
        
        LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
        
        dbgs() << "  Device kernel has " << std::distance(LI.begin(), LI.end()) 
                     << " top-level loops\n";
                         
        dbgs() << "Processing device kernel: " << funcName << "\n";
    }
    
    return PreservedAnalyses::none();
}
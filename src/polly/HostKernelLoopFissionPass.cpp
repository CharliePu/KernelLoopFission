#include "polly/HostKernelLoopFissionPass.h"
#include "common/helper.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/IRBuilder.h"

#define DEBUG_TYPE "host-kernel-loop-fission"

using namespace llvm;

PreservedAnalyses HostKernelLoopFissionPass::run(Module &M, ModuleAnalysisManager &MAM) {
    dbgs() << "Running HostKernelLoopFissionPass\n";
    
    FunctionAnalysisManager &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    
    // Process each function - looking for functions that:
    // 1. Contain "__device_stub__" in the demangled name
    // 2. Contain a kernel name from our candidate list as substring
    for (Function &F : M) {
        std::string functionName = extractFunctionName(F.getName());
        
        if (!isCudaKernelDeviceStub(&F))
            continue;
            
        bool isCandidate = false;
        for (const auto &kernel : Result.candidateFunctions) {
            std::string kernelName = extractFunctionName(kernel);
            
            if (functionName.find(kernelName) != std::string::npos) {
                isCandidate = true;
                break;
            }
        }
        
        if (!isCandidate)
            continue;
            
        dbgs() << "Transforming host stub for function: " << functionName << "\n";
        
        LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
        
        // In a real implementation, we would now transform the host stub
        dbgs() << "  Host stub has " << std::distance(LI.begin(), LI.end())
                     << " top-level loops\n";
        
        dbgs() << "Processing host stub: " << functionName << "\n";
    }
    
    return PreservedAnalyses::none();
}
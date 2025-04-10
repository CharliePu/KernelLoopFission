#ifndef LLVM_POLLY_LOOP_FISSION_DEPENDENCY_CHECK_PASS_H
#define LLVM_POLLY_LOOP_FISSION_DEPENDENCY_CHECK_PASS_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "polly/DependencyCheckResult.h"

// Helper structure to track kernel configuration information
struct CudaKernelConfig {
    llvm::Value *GridDimX = nullptr;
    llvm::Value *GridDimY = nullptr;
    llvm::Value *BlockDimX = nullptr;
    llvm::Value *BlockDimY = nullptr;
    llvm::CallInst *ConfigCall = nullptr;
};

namespace llvm {

class LoopFissionDependencyCheckPass : public PassInfoMixin<LoopFissionDependencyCheckPass> {
public:
    explicit LoopFissionDependencyCheckPass(DependencyCheckResult &result) 
        : Result(result) {}
    
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

private:
    CudaKernelConfig findKernelConfig(CallInst *kernelCall);
    std::vector<CallInst *> findCallers(Function *stubFunc);
    void createSerializeLoop(CallInst *kernelCall, CudaKernelConfig &config);
    void serializeKernelCalls(Function *kernelFunc, Function *stubFunc);
    
    // Separate an instruction into its own basic block
    BasicBlock *separateInstAsBB(Instruction *inst);
    
    // Create a loop around a region of code, return the entry and exit blocks
    std::pair<BasicBlock*, BasicBlock*> wrapWithLoop(
        BasicBlock *beginBB, BasicBlock *endBB, Value *tripCount);
    
    // Replace a stub function call with a direct kernel call
    // Returns the new kernel call instruction
    CallInst *replaceStubWithKernel(CallInst *stubCall, Function *kernelFunc);
    
    DependencyCheckResult &Result;
};

} // namespace llvm

#endif // LLVM_POLLY_LOOP_FISSION_DEPENDENCY_CHECK_PASS_H
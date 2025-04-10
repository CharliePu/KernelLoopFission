#ifndef LLVM_POLLY_HOST_KERNEL_LOOP_FISSION_PASS_H
#define LLVM_POLLY_HOST_KERNEL_LOOP_FISSION_PASS_H

#include "llvm/IR/PassManager.h"
#include "polly/DependencyCheckResult.h"

namespace llvm {

class HostKernelLoopFissionPass : public PassInfoMixin<HostKernelLoopFissionPass> {
public:
    explicit HostKernelLoopFissionPass(const DependencyCheckResult &Result) : Result(Result) {}
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

private:
    const DependencyCheckResult &Result;
};

} // namespace llvm

#endif // LLVM_POLLY_HOST_KERNEL_LOOP_FISSION_PASS_H
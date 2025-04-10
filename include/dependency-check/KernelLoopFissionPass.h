#ifndef KERNEL_LOOP_FISSION_PASS_H
#define KERNEL_LOOP_FISSION_PASS_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class KernelLoopFissionPass : public PassInfoMixin<KernelLoopFissionPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif // KERNEL_LOOP_FISSION_PASS_H

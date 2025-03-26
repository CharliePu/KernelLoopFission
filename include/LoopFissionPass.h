// LoopFissionPass.h
#ifndef LOOP_FISSION_PASS_H
#define LOOP_FISSION_PASS_H

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "polly/PolyhedralInfo.h"

namespace llvm {
class LoopFissionPass : public FunctionPass {
public:

  struct LoopBounds {
    Value *start, *end, *mid;
    PHINode *indVar;
  };


  static char ID;
  LoopFissionPass();
  virtual ~LoopFissionPass();

  bool prepareLoopBounds(Loop *loop, LoopBounds &bounds);
  // bool splitLoop(Loop *loop, LoopFissionPass::LoopBounds &bounds, 
  //   BasicBlock *&loop1Preheader, BasicBlock *&loop2Preheader);
  bool splitLoop(Loop *loop, LoopFissionPass::LoopBounds &bounds,
                                  Loop *&loop1, Loop *&loop2);

  bool kernelFission(Function &F, Loop *loop1, Loop *loop2);

  bool addStreamModeArg(Function &F, Function **modifiedFunc);

  bool runOnFunction(Function &F) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};
}  // namespace llvm

#endif // LOOP_FISSION_PASS_H
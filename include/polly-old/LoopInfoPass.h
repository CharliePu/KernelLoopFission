// LoopInfoPass.h
#ifndef LOOP_INFO_PASS_H
#define LOOP_INFO_PASS_H

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "polly/PolyhedralInfo.h"
#include "llvm/ADT/DenseMap.h"

namespace llvm {
class LoopInfoPass : public FunctionPass {
public:
  static char ID;
  LoopInfoPass();
  virtual ~LoopInfoPass();

  bool runOnFunction(Function &F) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

  Loop* getSplittableLoop(Function * function);

private:
  static SmallDenseMap<Function *, Loop *> CandidateLoops;
  static SmallDenseMap<Loop *, LoopInfo *> LoopInfoMap;
};
}  // namespace llvm

#endif // LOOP_INFO_PASS_H
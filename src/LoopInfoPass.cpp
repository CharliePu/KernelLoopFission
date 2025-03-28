// LoopInfoPass.cpp
#include "LoopInfoPass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "polly/ScopInfo.h"

#include "helper.h"

using namespace llvm;
using namespace polly;

SmallDenseMap<Function *, Loop *> LoopInfoPass::CandidateLoops; 
SmallDenseMap<Loop *, LoopInfo *> LoopInfoPass::LoopInfoMap;

char LoopInfoPass::ID = 0;

LoopInfoPass::LoopInfoPass() : LoopPass(ID) {}

LoopInfoPass::~LoopInfoPass() {}

bool LoopInfoPass::runOnLoop(Loop *L, LPPassManager &LPM) {

  auto func = (*L->block_begin())->getParent();
  if (!isCudaKernel(func)) {
    return false;
  }

  if (!L->isOutermost()) {
    return false;
  }

  PolyhedralInfo &PI = getAnalysis<PolyhedralInfo>();
  
  auto *S = PI.getScopContainingLoop(L);
  if (S) {
    dbgs() << "\tScop: " << S->getNameStr() << "\n";
  } else {
    dbgs() << "\tNo static control part found\n";
  }

  // dbgs()<<"Print info start\n";
  // PI.print(dbgs());
  // dbgs()<<"Print info end\n";
  
  if (PI.isParallel(L)) {
    dbgs()<<"\tParallel loop: "+L->getHeader()->getName()<<"\n";
  } else {
    dbgs()<<"\tNon-parallel loop: "+L->getHeader()->getName()<<"\n";
  }
  
  // for (auto *BB : L->blocks()) {
  //   dbgs() << "\t\tBasic Block: " << BB->getName() << "\n";
  //   for (auto &I : *BB) {
  //     dbgs() <<"\t\t\t"<< I << "\n";
  //   }
  // }
  
  dbgs() << "\n\n";

  // Placeholder, assuming every loop is splittable
  // CandidateLoops[L->getHeader()->getParent()] = L;
  
  return false;
}

void LoopInfoPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<PolyhedralInfo>();
  AU.setPreservesAll();
}

Loop *llvm::LoopInfoPass::getSplittableLoop(Function *function)
{
  if (auto it = CandidateLoops.find(function); it != CandidateLoops.end()) {
    return it->second;
  }

  return nullptr;
}

// Register the pass
static RegisterPass<LoopInfoPass> X("loop-info", "Loop Information Pass");
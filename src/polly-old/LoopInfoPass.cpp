// LoopInfoPass.cpp
#include "polly-old/LoopInfoPass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "polly/ScopInfo.h"
#include "polly/DependenceInfo.h"

#include "common/helper.h"

using namespace llvm;
using namespace polly;

SmallDenseMap<Function *, Loop *> LoopInfoPass::CandidateLoops; 
SmallDenseMap<Loop *, LoopInfo *> LoopInfoPass::LoopInfoMap;

LoopInfoPass::LoopInfoPass() : FunctionPass(ID) {}

LoopInfoPass::~LoopInfoPass() {}

bool LoopInfoPass::runOnFunction(Function &F) {
  LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  
  PolyhedralInfo &PI = getAnalysis<PolyhedralInfo>();

  for (Loop *L : LI) {
    auto func = (*L->block_begin())->getParent();

    dbgs() << "Loop: " << L->getHeader()->getName() << "\n";
    dbgs() << "\tLoop depth: " << L->getLoopDepth() << "\n";
    dbgs() << "\tOuter loop?: " << L->isOutermost() << "\n";
    
    auto *S = const_cast<Scop*>(PI.getScopContainingLoop(L));
    if (S) {
      dbgs() << "\tScop: Yes\n"; //<< S->getNameStr() << "\n";
    } else {
      dbgs() << "\tScop: no\n";
    }


    dbgs() << "\tParallel?: ";
    if (S && PI.isParallel(L)) {
      dbgs() << "yes\n";
    } else {
      dbgs() << "no\n";
    }
  }
  
  return false;
}


void LoopInfoPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<PolyhedralInfo>();
  AU.addRequired<DependenceInfoWrapperPass>();
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
char LoopInfoPass::ID = 0;
static RegisterPass<LoopInfoPass> X("loop-info", "Loop Information Pass");
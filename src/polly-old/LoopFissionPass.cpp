// LoopFissionPass.cpp
#include "polly-old/LoopFissionPass.h"
#include "polly-old/LoopInfoPass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"

#include "common/helper.h"

using namespace llvm;

char LoopFissionPass::ID = 0;

LoopFissionPass::LoopFissionPass() : FunctionPass(ID) {}

LoopFissionPass::~LoopFissionPass() {}

bool LoopFissionPass::prepareLoopBounds(Loop *loop, LoopBounds &bounds)
{

    auto &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
    PHINode *indVar = loop->getInductionVariable(SE);
    if (!indVar)
    {
        return false;
    }

    bounds.indVar = indVar;
    bounds.start = indVar->getIncomingValueForBlock(loop->getLoopPreheader());

    // Get end from the expression indVar < end or end < indVar
    BranchInst *BI = dyn_cast<BranchInst>(loop->getLoopLatch()->getTerminator());
    if (!BI || !BI->isConditional())
        return false;

    ICmpInst *cmp = dyn_cast<ICmpInst>(BI->getCondition());
    if (!cmp)
        return false;

    Value *op0 = cmp->getOperand(0);
    Value *op1 = cmp->getOperand(1);
    if (op0 == indVar)
    {
        bounds.end = op1;
    }
    else
    {
        bounds.end = op0;
    }

    // Build midpoint as (start + end) / 2
    IRBuilder<> builder(loop->getHeader()->getContext());
    builder.SetInsertPoint(loop->getLoopPreheader()->getTerminator());
    Value *sum = builder.CreateAdd(bounds.start, bounds.end);
    bounds.mid = builder.CreateSDiv(sum, builder.getInt32(2), "fission_loop_midpoint");

    return true;
}

/*
bool LoopFissionPass::splitLoop(Loop *loop, LoopFissionPass::LoopBounds &bounds,
                                BasicBlock *&loop1Preheader, BasicBlock *&loop2Preheader) {
    // Clone the loop
    SmallVector<BasicBlock*, 8> clonedBlocks;
    ValueToValueMapTy VMap;
    Function *func = loop->getHeader()->getParent();
    for (auto BB : loop->blocks()) {
        auto cloneBB = CloneBasicBlock(BB, VMap,
            BB->getName() + ".split",
            func);
        clonedBlocks.push_back(cloneBB);
        VMap[BB] = cloneBB;
    }

    // Fix value references in the cloned loop, restore cfg
    for (auto BB : clonedBlocks) {
        for (auto &I : *BB) {
            RemapInstruction(&I, VMap,
                RF_NoModuleLevelChanges | RF_IgnoreMissingLocals);
        }
    }

    // Insert the cloned loop to the existing cfg
    auto loop1Header = loop->getHeader();
    auto loop1Latch = loop->getLoopLatch();
    auto loop1Exit = loop->getExitBlock();
    auto loop2Header = cast<BasicBlock>(VMap[loop1Header]);
    auto loop2Latch = cast<BasicBlock>(VMap[loop1Latch]);
    auto loop2Exit = cast<BasicBlock>(VMap[loop1Exit]);

    loop1Exit->getTerminator()->replaceUsesOfWith(loop2Header, loop2Exit);
    loop2Header->getTerminator()->replaceUsesOfWith(loop1Exit, loop1Exit);

    // Change the loop bounds: loop1: start <= mid, loop2: mid < end
    auto loop1IndVar = bounds.indVar;
    auto loop2IndVar = cast<PHINode>(VMap[loop1IndVar]);
    loop2IndVar->addIncoming(bounds.mid, loop2Preheader); // TODO: we want to replace old, not add?
    loop1IndVar->addIncoming(bounds.start, loop1Preheader);

    loop1Preheader = loop->getLoopPreheader();
    loop2Preheader = cast<BasicBlock>(VMap[loop1Preheader]);

    return true;
}
    */

bool LoopFissionPass::splitLoop(Loop *loop, LoopBounds &bounds,
                                Loop *&loop1, Loop *&loop2)
{

    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();

    ValueToValueMapTy VMap;
    SmallVector<BasicBlock *, 8> clonedBlocks;
    loop1 = loop;
    loop2 = cloneLoopWithPreheader(
        loop->getExitBlock(), loop->getHeader(), loop, VMap, ".split", &LI, &DT,
        clonedBlocks);

    BranchInst *loop1Branch = dyn_cast<BranchInst>(loop->getLoopLatch()->getTerminator());

    // loop 1 condition e.g. start <= mid
    ICmpInst *origCmp = dyn_cast<ICmpInst>(loop1Branch->getCondition());
    IRBuilder<> Builder(origCmp);
    Value *newCond = Builder.CreateICmp(
        origCmp->getPredicate(),
        origCmp->getOperand(0),
        bounds.mid,
        "loopexit.mid");
    loop1Branch->setCondition(newCond);
    
    // loop 2 condition e.g. mid < end, need to initialize induction variable to mid
    PHINode *loop1IndVar = bounds.indVar;
    PHINode *loop2Indvar = cast<PHINode>(VMap[loop1IndVar]);
    for (unsigned i = 0; i < loop2Indvar->getNumIncomingValues(); ++i) {
        if (loop2Indvar->getIncomingBlock(i) == loop2->getLoopPreheader()) {
            loop2Indvar->setIncomingValue(i, bounds.mid);
            break;
        }
    }

    return true;
}

bool LoopFissionPass::kernelFission(Function &F, Loop *loop1, Loop *loop2) {

    BasicBlock *loop1Preheader = loop1->getLoopPreheader();
    BasicBlock *loop2Preheader = loop2->getLoopPreheader();
    if (!loop1Preheader || !loop2Preheader) {
        return false;
    }

    // Get the function's entry block
    BasicBlock *entryBlock = &F.getEntryBlock();

    // Check the entry block's terminator
    BranchInst *entryBr = dyn_cast<BranchInst>(entryBlock->getTerminator());
    if (!entryBr || !entryBr->isUnconditional() || entryBr->getSuccessor(0) != loop1Preheader) {
        return false;
    }

    // Insert conditional check block before loop1
    LLVMContext &Context = F.getContext();
    BasicBlock *condCheck = BasicBlock::Create(Context, "streamModeCheck", &F, loop1Preheader);

    // Build conditional check block
    IRBuilder<> builder(condCheck);
    builder.SetInsertPoint(condCheck);
    Argument *streamModeArg = &*(F.arg_end() - 1); // Last argument
    Value *cmp = builder.CreateICmpEQ(streamModeArg, builder.getInt32(0), "streamMode.cmp");
    builder.CreateCondBr(cmp, loop1->getHeader(), loop2->getHeader());
    
    // Make old preheader branch to the conditional check
    auto loop1PreheaderTerm = loop1Preheader->getTerminator();
    loop1PreheaderTerm->replaceUsesOfWith(loop1Preheader, condCheck);


    // Branch loop1's exit to the loop2's exit block
    auto loop2Exit = loop2->getExitBlock();
    auto loop1Exit = loop1->getExitBlock();
    auto loop1ExitingBlock = loop1->getExitingBlock();
    loop1ExitingBlock->getTerminator()->replaceUsesOfWith(loop1Exit, loop2Exit);

    return true;
}

bool LoopFissionPass::addStreamModeArg(Function &F, Function **modifiedFunc)
{
    // Create the new function with streamMode argument
    SmallVector<Type*, 1> argTypes;
    for (auto& arg : F.args())
        argTypes.push_back(arg.getType());
    argTypes.push_back(Type::getInt32Ty(F.getContext())); // streamMode
    FunctionType* newFuncType = FunctionType::get(F.getReturnType(), argTypes, F.isVarArg());

    Function *FNew = Function::Create(newFuncType, F.getLinkage(), F.getAddressSpace(), F.getName() + ".fission", F.getParent());

    
    // Clone the function contents
    ValueToValueMapTy VMap;
    auto newIt = FNew->arg_begin();
    for (auto &arg : F.args()) {
        newIt->setName(arg.getName());
        VMap[&arg] = &*newIt;
        ++newIt;
    }
    Argument *extraArg = &*newIt;
    extraArg->setName("streamMode"); 

    SmallVector<ReturnInst*, 8> Returns;
    CloneFunctionInto(FNew, &F, VMap, CloneFunctionChangeType::LocalChangesOnly, Returns);

    *modifiedFunc = FNew;

    return true;
}

bool LoopFissionPass::runOnFunction(Function &F)
{
    if (!isCudaKernel(&F))
    {
        return false;
    }

    LoopInfoPass &LIP = getAnalysis<LoopInfoPass>();
    Loop *loop = LIP.getSplittableLoop(&F);

    LoopBounds bounds;
    if (!prepareLoopBounds(loop, bounds))
    {
        return false;
    }

    Loop *loop1, *loop2;
    if (!splitLoop(loop, bounds, loop1, loop2))
    {
        return false;
    }

    Function *modifiedFunc;
    if (!addStreamModeArg(F, &modifiedFunc))
    {
        return false;
    }

    if (!kernelFission(*modifiedFunc, loop1, loop2))
    {
        return false;
    }

    return false;
}

void LoopFissionPass::getAnalysisUsage(AnalysisUsage &AU) const
{
    AU.addRequired<LoopInfoPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.setPreservesAll();
}

// Register the pass
static RegisterPass<LoopFissionPass> X("loop-fission", "Loop Fission Pass");
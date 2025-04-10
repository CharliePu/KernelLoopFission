#include "polly/LoopFissionDependencyCheckPass.h"
#include "polly/LoopFissionDependencyAnalysis.h"
#include "common/helper.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#define DEBUG_TYPE "loop-fission-dependency-check"

using namespace llvm;

// Forward declaration
bool isPotentialLoopHeader(BasicBlock *BB);

// Check if a basic block is a potential loop header
bool isPotentialLoopHeader(BasicBlock *BB)
{
    // A potential loop header has a predecessor that it dominates
    for (BasicBlock *Pred : predecessors(BB))
    {
        // Self-loop (rare but possible)
        if (BB == Pred)
        {
            return true;
        }

        // Check for back edges using a simple heuristic
        // This is a simplified check and would be better with proper dominator info
        for (BasicBlock *Succ : successors(Pred))
        {
            if (Succ == BB)
            {
                // Check if there's a path from BB to Pred (indicating a loop)
                std::set<BasicBlock *> visited;
                std::queue<BasicBlock *> workList;

                workList.push(BB);

                while (!workList.empty())
                {
                    BasicBlock *Current = workList.front();
                    workList.pop();

                    if (Current == Pred)
                    {
                        return true; // Found a path from BB to Pred
                    }

                    if (visited.insert(Current).second)
                    {
                        for (BasicBlock *Succ : successors(Current))
                        {
                            workList.push(Succ);
                        }
                    }
                }
            }
        }
    }

    return false;
}

// Checks if a CUDA kernel function contains at least one loop
bool cudaKernelContainsLoop(Function *F)
{
    if (!F || !isCudaKernel(F))
    {
        return false;
    }

    // Look for basic blocks that form a loop
    for (BasicBlock &BB : *F)
    {
        // If there's a backward edge in the CFG, it's likely a loop
        if (isPotentialLoopHeader(&BB))
        {
            return true;
        }
    }

    return false;
}

// Find corresponding host/device function pairs in the module
std::vector<std::pair<Function *, Function *>> findHostDeviceFunctionPairs(Module &M)
{
    std::vector<std::pair<Function *, Function *>> functionPairs;
    std::map<std::string, Function *> deviceFunctions;
    std::map<std::string, Function *> hostStubFunctions;

    // First, identify all CUDA kernel functions and device stub functions
    for (Function &F : M)
    {
        std::string name = F.getName().str();

        if (isCudaKernel(&F))
        {
            deviceFunctions[name] = &F;
            dbgs() << "Found CUDA kernel: " << name << "\n";
        }
        else if (isCudaKernelDeviceStub(&F))
        {
            // For stubs, store the kernel name without __device_stub__ prefix as the key
            size_t pos = name.find("__device_stub__");
            if (pos != std::string::npos)
            {
                // Remove the __device_stub__ prefix from the name to get the original kernel name
                std::string kernelName = name.substr(pos + 15); // 15 is length of "__device_stub__"

                // Insert stub with original kernel name as key
                hostStubFunctions[kernelName] = &F;
                dbgs() << "Found device stub for kernel: " << kernelName << "\n";
            }
        }
    }

    // Match device functions with their host stubs
    for (auto &[kernelName, deviceFunc] : deviceFunctions)
    {
        for (auto &[stubName, hostFunc] : hostStubFunctions)
        {
            if (kernelName.find(stubName) != std::string::npos)
            {
                // Found a match
                functionPairs.emplace_back(deviceFunc, hostFunc);
                dbgs() << "Matched device function " << kernelName
                       << " with host stub " << stubName << "\n";
            }
        }
    }

    return functionPairs;
}

CudaKernelConfig LoopFissionDependencyCheckPass::findKernelConfig(CallInst *kernelCall)
{
    CudaKernelConfig config;

    // Get the parent function
    Function *F = kernelCall->getFunction();

    // Track visited blocks to avoid cycles
    std::set<BasicBlock *> visited;
    std::queue<BasicBlock *> worklist;

    // Start with the kernel call's block
    BasicBlock *startBB = kernelCall->getParent();

    // Add all predecessors of the start block
    for (auto *pred : predecessors(startBB))
    {
        worklist.push(pred);
    }

    while (!worklist.empty())
    {
        BasicBlock *currentBB = worklist.front();
        worklist.pop();

        if (visited.count(currentBB))
        {
            continue;
        }

        visited.insert(currentBB);

        // Check all calls in this block
        for (auto &I : *currentBB)
        {
            if (CallInst *call = dyn_cast<CallInst>(&I))
            {
                Function *callee = call->getCalledFunction();
                if (callee && callee->getName() == "__cudaPushCallConfiguration")
                {
                    dbgs() << "Found cudaPushCallConfiguration: " << *call << "\n";
                    config.ConfigCall = call;
                    if (call->arg_size() >= 4)
                    {
                        config.GridDimX = call->getArgOperand(0);
                        config.GridDimY = call->getArgOperand(1);
                        config.BlockDimX = call->getArgOperand(2);
                        config.BlockDimY = call->getArgOperand(3);

                        dbgs() << "  GridDimX: " << *config.GridDimX << "\n";
                        dbgs() << "  GridDimY: " << *config.GridDimY << "\n";
                        dbgs() << "  BlockDimX: " << *config.BlockDimX << "\n";
                        dbgs() << "  BlockDimY: " << *config.BlockDimY << "\n";
                    }
                    return config;
                }
            }
        }

        // Add predecessors to worklist
        for (auto *pred : predecessors(currentBB))
        {
            worklist.push(pred);
        }
    }

    return config;
}

std::vector<CallInst *> LoopFissionDependencyCheckPass::findCallers(Function *stubFunc)
{
    std::vector<CallInst *> callInsts;
    auto M = stubFunc->getParent();

    for (auto &F : *M)
    {
        for (auto &BB : F)
        {
            for (auto &I : BB)
            {
                if (auto *call = dyn_cast<CallInst>(&I))
                {
                    if (call->getCalledFunction() == stubFunc)
                    {
                        callInsts.push_back(call);
                    }
                }
            }
        }
    }

    return callInsts;
}

void LoopFissionDependencyCheckPass::createSerializeLoop(CallInst *kernelCall, CudaKernelConfig &config)
{
    dbgs() << "Creating serialize loop for kernel call: " << *kernelCall << "\n";

    // Collect the dimensions to create loops for (in reverse order of nesting)
    std::vector<Value *> iterateDims;
    iterateDims.push_back(config.BlockDimY);
    iterateDims.push_back(config.BlockDimX);
    iterateDims.push_back(config.GridDimY);
    iterateDims.push_back(config.GridDimX);

    // Separate kernel call into its own basic block
    auto kernelCallBB = separateInstAsBB(kernelCall);

    // // Starting point for wrapping loops
    // auto wrapBeginBB = kernelCallBB;
    // auto wrapEndBB = kernelCallBB;

    // // Create nested loops for each dimension
    // for (auto dim : iterateDims)
    // {
    //     // Create a loop that wraps the current region
    //     // Like: for (unsigned int i = 0; i < dim; i++) { ... }
    //     std::tie(wrapBeginBB, wrapEndBB) = wrapWithLoop(wrapBeginBB, wrapEndBB, dim);
    // }

    // InlineFunctionInfo inlineInfo;
    // auto result = InlineFunction(*kernelCall, inlineInfo);
    // if (!result.isSuccess())
    // {
    //     dbgs() << "Failed Inlined kernel call: " << *kernelCall << "\n";
    //     dbgs() << "Reason: " << result.getFailureReason() << "\n";
    // }

    // CallInst *kernelCall = replaceStubWithKernel(stubCall, kernelFunc);

    dbgs() << "Created serialized loop for kernel execution\n";
}

void LoopFissionDependencyCheckPass::serializeKernelCalls(Function *kernelFunc, Function *stubFunc)
{
    // Find all callers of the device stub function
    auto callInsts = findCallers(stubFunc);
    if (callInsts.empty())
    {
        dbgs() << "No kernel calls found for stub: " << stubFunc->getName() << "\n";
        return;
    }

    if (callInsts.size() > 1)
    {
        dbgs() << "Multiple kernel calls found for stub: " << stubFunc->getName() << "\n";
    }

    // Process each call to the stub function
    for (auto *stubCall : callInsts)
    {
        // Find the kernel configuration (grid/block dimensions)
        CudaKernelConfig config = findKernelConfig(stubCall);
        if (!config.ConfigCall)
        {
            dbgs() << "No configuration found for kernel call: " << *stubCall << "\n";
            continue;
        }


        // Wrap the kernel call with nested loops to simulate GPU execution
        createSerializeLoop(stubCall, config);
    }
}

PreservedAnalyses LoopFissionDependencyCheckPass::run(Module &M, ModuleAnalysisManager &MAM)
{
    dbgs() << "Running LoopFissionDependencyCheckPass\n";

    FunctionAnalysisManager &FAM =
        MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

    std::vector<std::pair<Function *, Function *>> hostDeviceFunctionPairs =
        findHostDeviceFunctionPairs(M);

    for (auto &[deviceFunc, hostFunc] : hostDeviceFunctionPairs)
    {
        if (!cudaKernelContainsLoop(deviceFunc))
        {
            continue;
        }

        serializeKernelCalls(deviceFunc, hostFunc);

        Result.candidateFunctions.push_back(deviceFunc->getName().str());
    }

    return PreservedAnalyses::none();
}

BasicBlock *LoopFissionDependencyCheckPass::separateInstAsBB(Instruction *inst)
{
    // Get the current basic block containing the instruction
    BasicBlock *origBB = inst->getParent();

    origBB->splitBasicBlockBefore(inst);

    origBB->splitBasicBlock(inst);

    origBB->setName(".separated");

    dbgs() << "Created separated BB for instruction: " << *inst << "\n";

    return origBB;
}

std::pair<BasicBlock *, BasicBlock *> LoopFissionDependencyCheckPass::wrapWithLoop(
    BasicBlock *beginBB, BasicBlock *endBB, Value *tripCount)
{

    Function *F = beginBB->getParent();
    LLVMContext &ctx = F->getContext();

    // Get a name hint for this loop based on which dimension we're currently wrapping
    // Each kernel gets wrapped with blockDimY, blockDimX, gridDimY, gridDimX (in that order)
    static int loopCounter = 0;
    std::string dimName;
    switch (loopCounter % 4)
    {
    case 0:
        dimName = "blockDimY";
        break;
    case 1:
        dimName = "blockDimX";
        break;
    case 2:
        dimName = "gridDimY";
        break;
    case 3:
        dimName = "gridDimX";
        break;
    }
    loopCounter++;

    // Create basic blocks for the loop with descriptive names
    BasicBlock *preheaderBB = BasicBlock::Create(ctx, dimName + ".loop.ph", F, beginBB);
    BasicBlock *headerBB = BasicBlock::Create(ctx, dimName + ".loop.header", F, beginBB);
    BasicBlock *bodyBB = beginBB; // The existing begin block becomes the loop body
    BasicBlock *latchBB = BasicBlock::Create(ctx, dimName + ".loop.latch", F, nullptr);
    BasicBlock *exitBB = BasicBlock::Create(ctx, dimName + ".loop.exit", F, nullptr);

    // Create induction variable and its initialization in the preheader
    IRBuilder<> builder(preheaderBB);
    Type *idxType = Type::getInt32Ty(ctx);
    Value *zero = ConstantInt::get(idxType, 0);

    // Create a PHI node for the loop counter in the header
    builder.SetInsertPoint(headerBB);
    PHINode *counterPHI = builder.CreatePHI(idxType, 2, dimName + ".idx");

    // Create the loop condition (counter < tripCount)
    Value *tripCountCast = builder.CreateZExtOrTrunc(tripCount, idxType, dimName + ".count");
    Value *exitCond = builder.CreateICmpULT(counterPHI, tripCountCast, dimName + ".cond");
    builder.CreateCondBr(exitCond, bodyBB, exitBB);

    // Create the increment in the latch block and branch back to header
    builder.SetInsertPoint(latchBB);
    Value *nextCounter = builder.CreateAdd(counterPHI, ConstantInt::get(idxType, 1), dimName + ".next");
    builder.CreateBr(headerBB);

    // Connect the end of the body to the latch
    builder.SetInsertPoint(endBB);
    Instruction *endInst = endBB->getTerminator();
    if (endInst)
    {
        endInst->eraseFromParent();
    }
    builder.CreateBr(latchBB);

    // Update the PHI node
    counterPHI->addIncoming(zero, preheaderBB);
    counterPHI->addIncoming(nextCounter, latchBB);

    // Connect the entry to the preheader
    for (auto *pred : predecessors(beginBB))
    {
        if (pred == headerBB)
            continue; // Don't update the loop header's branch

        Instruction *termInst = pred->getTerminator();
        for (unsigned i = 0; i < termInst->getNumSuccessors(); ++i)
        {
            if (termInst->getSuccessor(i) == beginBB)
            {
                termInst->setSuccessor(i, preheaderBB);
            }
        }
    }

    // Connect the preheader to the header
    builder.SetInsertPoint(preheaderBB);
    builder.CreateBr(headerBB);

    dbgs() << "Created " << dimName << " loop with trip count: " << *tripCount << "\n";

    return std::make_pair(preheaderBB, exitBB);
}

CallInst *LoopFissionDependencyCheckPass::replaceStubWithKernel(CallInst *stubCall, Function *kernelFunc)
{
    dbgs() << "Replacing stub call: " << *stubCall << " with direct kernel call\n";
    dbgs() << "  Stub name: " << stubCall->getCalledFunction()->getName() << "\n";
    dbgs() << "  Kernel name: " << kernelFunc->getName() << "\n";

    // Get the original arguments from the stub call
    SmallVector<Value *, 8> args;
    for (unsigned i = 0; i < stubCall->arg_size(); ++i)
    {
        args.push_back(stubCall->getArgOperand(i));
    }

    // Create a new call instruction to the kernel function
    auto kernelCall = CallInst::Create(kernelFunc, args);

    // Copy attributes and metadata
    kernelCall->setCallingConv(stubCall->getCallingConv());
    kernelCall->setAttributes(stubCall->getAttributes());

    ReplaceInstWithInst(stubCall, kernelCall);

    dbgs() << "Replacing stub call with kernel call: " << *kernelCall << "\n";

    return kernelCall;
}
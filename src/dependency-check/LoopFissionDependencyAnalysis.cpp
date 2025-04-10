#include "dependency-check/LoopFissionDependencyAnalysis.h"

#include <queue>
#include <vector>
#include <unordered_set>

#include "llvm/Support/Debug.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/IR/Instructions.h"

// #define DEBUG_TYPE "loop-fission-dependency"

using namespace llvm;

AnalysisKey LoopFissionDependencyAnalysis::Key;

// // Manually find the induction variable of the loop
// // L->getInductionVariable doesn't work, don't understand why
// PHINode *findInductionVariable(Loop &L)
// {
//     BasicBlock *Header = L.getHeader();
//     BasicBlock *Latch = L.getLoopLatch();

//     if (!Header || !Latch)
//         return nullptr;

//     for (PHINode &PN : Header->phis())
//     {
//         // Check if this PHI has an incoming value from the latch
//         Value *StepVal = PN.getIncomingValueForBlock(Latch);
//         if (!StepVal)
//             continue;

//         // Look for a simple increment/addition pattern
//         if (BinaryOperator *BO = dyn_cast<BinaryOperator>(StepVal))
//         {
//             if (BO->getOpcode() == Instruction::Add)
//             {
//                 // Check if one operand is the PHI itself
//                 if (BO->getOperand(0) == &PN || BO->getOperand(1) == &PN)
//                 {
//                     // Check if PHI is used in a comparison
//                     for (User *U : PN.users())
//                     {
//                         if (ICmpInst *ICI = dyn_cast<ICmpInst>(U))
//                         {
//                             if (L.contains(ICI->getParent()))
//                             {
//                                 return &PN;
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     }

//     return nullptr;
// }

// Less restrictive, imprecise version
PHINode *findInductionVariable(Loop &L)
{
    BasicBlock *Header = L.getHeader();
    BasicBlock *Latch = L.getLoopLatch();

    if (!Header || !Latch)
        return nullptr;

    for (PHINode &PN : Header->phis())
    {
        // Check if this PHI has an incoming value from the latch
        Value *StepVal = PN.getIncomingValueForBlock(Latch);
        if (!StepVal)
            continue;

        // Look for a simple increment/addition pattern
        if (BinaryOperator *BO = dyn_cast<BinaryOperator>(StepVal))
        {
            if (BO->getOpcode() == Instruction::Add)
            {
                // Check if one operand is the PHI itself
                if (BO->getOperand(0) == &PN || BO->getOperand(1) == &PN)
                {
                    // First check if PHI is used directly in a comparison
                    for (User *U : PN.users())
                    {
                        if (ICmpInst *ICI = dyn_cast<ICmpInst>(U))
                        {
                            if (L.contains(ICI->getParent()))
                            {
                                return &PN;
                            }
                        }
                    }
                    
                    // Then check if the increment operation is used in a comparison
                    for (User *U : BO->users())
                    {
                        if (ICmpInst *ICI = dyn_cast<ICmpInst>(U))
                        {
                            if (L.contains(ICI->getParent()))
                            {
                                return &PN;
                            }
                        }
                    }
                    
                    // If we still haven't found a direct comparison use,
                    // but this PHI updates itself in a loop-like pattern,
                    // it's likely the induction variable
                    return &PN;
                }
            }
        }
    }

    return nullptr;
}

struct Candidate
{
    std::vector<std::pair<Value *, unsigned>> operators;
    std::vector<Value *> base_ptrs;
    bool no_memory;
};

struct AffineFunction
{
    Value *memory_inst;
    std::vector<std::pair<Value *, unsigned>> operators;
    std::vector<Value *> base_ptrs;
};

bool operatorsMatch(const std::vector<std::pair<Value *, unsigned>> &ops1,
                    const std::vector<std::pair<Value *, unsigned>> &ops2)
{
    if (ops1.size() != ops2.size())
        return false;

    for (size_t i = 0; i < ops1.size(); i++)
    {
        // Compare opcodes directly
        if (ops1[i].second != ops2[i].second)
            return false;
    }

    return true;
}

bool analyzeLoopDependencies(PHINode *indVar)
{

    // Start from induction variable
    std::queue<std::pair<Value *, Candidate>> workList;
    std::unordered_set<Value *> visited;


    Candidate initialCandidate;
    initialCandidate.operators.push_back({indVar, Instruction::PHI});
    initialCandidate.no_memory = false;
    workList.push({indVar, initialCandidate});

    std::vector<AffineFunction> affineFunctions;

    while (!workList.empty())
    {
        auto [value, candidate] = workList.front();
        workList.pop();

        if (visited.count(value))
        {
            continue;
        }
        visited.insert(value);

        if (auto *inst = dyn_cast<Instruction>(value))
        {
            dbgs() << "Processing: " << *inst << "\n";
            
            Candidate newCandidate = candidate;

            if (inst->isBinaryOp())
            {
                newCandidate.operators.push_back({inst, inst->getOpcode()});
            }
            else if (auto *call = dyn_cast<CallInst>(inst))
            {
                newCandidate.operators.push_back({call, inst->getOpcode()});
            }
            else if (auto *gep = dyn_cast<GetElementPtrInst>(inst))
            {
                newCandidate.operators.push_back({gep, Instruction::GetElementPtr});
                newCandidate.base_ptrs.push_back(gep->getPointerOperand());
            }
            else if (isa<PHINode>(inst))
            {
                // dbgs()<<"modifying phi"<<*inst<<"\n";
                // newCandidate.no_memory = true;
                newCandidate.operators.push_back({inst, Instruction::PHI});
            }
            else if (isa<LoadInst>(inst) || isa<StoreInst>(inst))
            {
                newCandidate.operators.push_back({inst, inst->getOpcode()});

                if (newCandidate.no_memory)
                {
                    dbgs() << "Induction variable used in memory op after phi - not parallelizable\n";
                    dbgs() << "Path: ";
                    for (const auto &op : newCandidate.operators)
                    {
                        dbgs() << *op.first << " ";
                    }
                    return false;
                }

                AffineFunction function;
                function.memory_inst = inst;
                function.operators = newCandidate.operators;
                function.base_ptrs = newCandidate.base_ptrs;

                affineFunctions.push_back(function);
            }

            dbgs() << "users: ";
            {
                if (auto branch = dyn_cast<BranchInst>(inst))
                {
                    for (auto *succ : branch->successors())
                    {
                        // Check if succ->front() is a load instruction
                        if (auto loadInst = dyn_cast<LoadInst>(&succ->front()))
                        {
                            // Only add to workList if inst is the address operand, not the loaded value
                            if (loadInst->getPointerOperand() == inst)
                            {
                                workList.push({&succ->front(), newCandidate});
                                dbgs() << succ->front() << "\n";
                            }
                        }
                        // Check if succ->front() is a store instruction
                        else if (auto storeInst = dyn_cast<StoreInst>(&succ->front()))
                        {
                            // Only add to workList if inst is the address operand, not the stored value
                            if (storeInst->getPointerOperand() == inst)
                            {
                                workList.push({&succ->front(), newCandidate});
                                dbgs() << succ->front() << "\n";
                            }
                        }
                        // For other instruction types, keep the original behavior
                        else
                        {
                            workList.push({&succ->front(), newCandidate});
                            dbgs() << succ->front() << "\n";
                        }
                    }
                }
                else
                {
                    for (auto *user : inst->users())
                    {
                        
                        // Check if user is a load instruction
                        if (auto loadInst = dyn_cast<LoadInst>(user))
                        {
                            // Only add to workList if inst is the address operand, not the loaded value
                            if (loadInst->getPointerOperand() == inst)
                            {
                                workList.push({user, newCandidate});
                                dbgs() << *user << "\n";
                            }
                        }
                        // Check if user is a store instruction
                        else if (auto storeInst = dyn_cast<StoreInst>(user))
                        {
                            // Only add to workList if inst is the address operand, not the stored value
                            if (storeInst->getPointerOperand() == inst)
                            {
                                workList.push({user, newCandidate});
                                dbgs() << *user << "\n";
                            }
                        }
                        // For other instruction types, keep the original behavior
                        else
                        {
                            workList.push({user, newCandidate});
                            dbgs() << *user << "\n";
                        }
                    }
                }
            }
            dbgs() << "\n";
        }
    }

    for (const auto &func : affineFunctions)
    {
        dbgs() << "Affine function: " << *func.memory_inst << "\n";
        dbgs() << "Operators: ";
        for (const auto &op : func.operators)
        {
            dbgs() << *op.first << " ";
        }
        dbgs() << "\nBase pointers: ";
        for (const auto *ptr : func.base_ptrs)
        {
            dbgs() << *ptr << " ";
        }
        dbgs() << "\n";
    }

    for (size_t i = 0; i < affineFunctions.size(); i++)
    {
        for (size_t j = i + 1; j < affineFunctions.size(); j++)
        {
            auto &func1 = affineFunctions[i];
            auto &func2 = affineFunctions[j];

            bool base_ptrs_match = false;
            for (auto *ptr1 : func1.base_ptrs)
            {
                for (auto *ptr2 : func2.base_ptrs)
                {
                    if (ptr1 == ptr2)
                    {
                        base_ptrs_match = true;
                        break;
                    }
                }
                if (base_ptrs_match)
                    break;
            }

            // If base pointers don't match, assume accesses are disjoint
            if (!base_ptrs_match)
                continue;

            if (!operatorsMatch(func1.operators, func2.operators))
            {
                // Operators don't match - might write to same location
                return false;
            }
        }
    }

    // If we get here, the loop can be parallelized
    return true;
}

LoopFissionDependencyAnalysis::Result
LoopFissionDependencyAnalysis::run(Loop &L, LoopAnalysisManager &LAM,
                                        LoopStandardAnalysisResults &AR)
{
    Result result;
    result.loop = &L;
    result.validLoop = false;

    dbgs() << "Loop: " << "\n";
    for (BasicBlock *BB : L.blocks())
    {
        dbgs() << *BB << "\n";
    }
    dbgs() << "\n";

    PHINode *indVar = findInductionVariable(L);
    if (!indVar)
    {
        dbgs() << "Loop does not have induction variable\n";
        return result;
    }
    dbgs() << "Induction variable: " << *indVar << "\n";

    result.validLoop = analyzeLoopDependencies(indVar);

    return result;
}
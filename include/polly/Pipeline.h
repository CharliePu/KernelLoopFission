#ifndef LLVM_POLLY_PIPELINE_H
#define LLVM_POLLY_PIPELINE_H

#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Passes/PassBuilder.h"

struct MyPipeline {
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;
    llvm::ModulePassManager MPM;
    llvm::FunctionPassManager FPM;
    llvm::LoopPassManager LPM;
    llvm::PassBuilder PB;
};

MyPipeline buildPipeline();

#endif // LLVM_POLLY_PIPELINE_H
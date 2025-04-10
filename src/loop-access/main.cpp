#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO/FunctionAttrs.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/MemorySSA.h"


#include "llvm/Transforms/Scalar/LoopPassManager.h"

#include "loop-access/ParallelLoopAnalysisPass.h"
#include "common/helper.h"

using namespace llvm;

static cl::OptionCategory ToolCategory("Parallel Loop Analysis Tool Options");
static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("<input LLVM IR file>"),
                                          cl::Required,
                                          cl::cat(ToolCategory),
                                          cl::value_desc("filename"));

int main(int argc, char **argv)
{
    InitLLVM X(argc, argv);
    cl::ParseCommandLineOptions(argc, argv, "LLVM Parallel Loop Analysis Driver\n");

    LLVMContext context;
    SMDiagnostic err;
    std::unique_ptr<Module> M = parseIRFile(InputFilename, err, context);

    if (!M)
    {
        err.print(argv[0], errs());
        return 1;
    }

    // Create analysis managers
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;
    PassBuilder PB;

    // Register standard passes
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    // Add required analysis passes but exclude vectorization
    FunctionPassManager FPM;

    // Add your core dependency analyses
    FAM.registerPass([&]
                     { return DominatorTreeAnalysis(); });
    FAM.registerPass([&]
                     { return PostDominatorTreeAnalysis(); });
    FAM.registerPass([&]
                     { return LoopAnalysis(); });
    FAM.registerPass([&]
                     { return ScalarEvolutionAnalysis(); });
    FAM.registerPass([&]
                     { return DependenceAnalysis(); });
    FAM.registerPass([&]
                     { return MemorySSAAnalysis(); });
    FAM.registerPass([&]
                     { return AAManager(); });

    // Register your custom analysis
    FAM.registerPass([&]
                     { return ParallelLoopAnalysis(); });

    // Add printing pass but avoid adding transformations
    FPM.addPass(ParallelLoopAnalysisPrinterPass());

    // For intrinsic functions, add a check before running your analysis
    for (auto &F : *M)
    {
        if (isCudaKernel(&F))
        {
            dbgs() << "Running Parallel Loop Analysis on function: " << F.getName() << "\n";
            FPM.run(F, FAM);
        }
    }

    return 0;
}
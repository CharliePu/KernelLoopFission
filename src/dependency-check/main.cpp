#include "llvm/Support/InitLLVM.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Utils/LoopSimplify.h"
#include "llvm/Transforms/Scalar/IndVarSimplify.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Linker/Linker.h"

#include "common/helper.h"
#include "dependency-check/LoopFissionDependencyAnalysis.h"
#include "dependency-check/KernelLoopFissionPass.h"


using namespace llvm;

static cl::OptionCategory toolCategory("Dependency Check Loop Analysis Tool Options");
static cl::opt<std::string> deviceIRFilePath(cl::Positional,
                                          cl::desc("<device LLVM IR file>"),
                                          cl::Required,
                                          cl::cat(toolCategory),
                                          cl::value_desc("device_filename"));

static cl::opt<std::string> hostIRPath("host",
                                          cl::desc("Optional host LLVM IR file to link with device IR"),
                                          cl::Optional,
                                          cl::cat(toolCategory),
                                          cl::value_desc("host_filename"));

// Load an IR file into a Module
std::unique_ptr<Module> loadIRFile(const std::string &filePath, LLVMContext &context) {
    SMDiagnostic err;
    auto module = parseIRFile(filePath, err, context);
    
    if (!module) {
        errs() << "Error loading IR file: " << filePath << "\n";
        err.print("", errs());
        exit(1);
    }
    
    return module;
}

// Combine two modules into one
std::unique_ptr<Module> combineModules(std::unique_ptr<Module> destModule, std::unique_ptr<Module> srcModule) {
    Linker::linkModules(*destModule, std::move(srcModule));
    return destModule;
}

int main(int argc, char **argv)
{
    InitLLVM x(argc, argv);
    cl::ParseCommandLineOptions(argc, argv, "LLVM Dependency Check Loop Analysis Driver\n");

    // Create context and load modules
    LLVMContext context;
    
    // Load device module first
    auto deviceModule = loadIRFile(deviceIRFilePath, context);

    // Create analysis managers
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;
    PassBuilder pb;
    

    pb.registerModuleAnalyses(MAM);
    pb.registerCGSCCAnalyses(CGAM);
    pb.registerFunctionAnalyses(FAM);
    pb.registerLoopAnalyses(LAM);
    pb.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    LAM.registerPass([&]
        { return LoopFissionDependencyAnalysis(); });
        

    FunctionPassManager fpm;
    fpm.addPass(PromotePass());

    fpm.addPass(LCSSAPass());

    LoopPassManager lpm;
    // lpm.addPass(IndVarSimplifyPass());
    fpm.addPass(createFunctionToLoopPassAdaptor(std::move(lpm)));

    fpm.addPass(LoopSimplifyPass());

    fpm.addPass(KernelLoopFissionPass());


    for (auto &f : *deviceModule)
    {
        if (isCudaKernel(&f))
        {
            fpm.run(f, FAM);
        }
    }

    // Save the modified module as output.ll
    std::error_code ec;
    raw_fd_ostream os("output.ll", ec);
    if (ec)
    {
        errs() << "Error opening output file: " << ec.message() << "\n";
        return 1;
    }
    deviceModule->print(os, nullptr);
    os.close();
    dbgs() << "Output written to output.ll\n";

    return 0;
}
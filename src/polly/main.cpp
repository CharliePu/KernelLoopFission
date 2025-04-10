#include "llvm/Support/InitLLVM.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <filesystem>

#include "common/helper.h"
#include "polly/LoopFissionDependencyAnalysis.h"
#include "polly/KernelLoopFissionPass.h"

#include "polly/RegisterPasses.h"
#include "polly/ScopDetection.h"
#include "polly/ScopInfo.h"
#include "polly/Simplify.h"
#include "polly/DependencyCheckResult.h"
#include "polly/ScopDetection.h"

#include "polly/Pipeline.h"
#include "polly/LoopFissionDependencyCheckPass.h"
#include "polly/HostKernelLoopFissionPass.h"
#include "polly/DeviceKernelLoopFissionPass.h"

#include <filesystem>

using namespace llvm;

static cl::OptionCategory toolCategory("Polly Loop Analysis Tool Options");
static cl::opt<std::string> hostIRPath(cl::Positional,
                                          cl::desc("<host LLVM IR file>"),
                                          cl::Required,
                                          cl::cat(toolCategory),
                                          cl::value_desc("host_filename"));
static cl::opt<std::string> deviceIRPath(cl::Positional,
                                         cl::desc("<device LLVM IR file>"),
                                         cl::Required,
                                         cl::cat(toolCategory),
                                         cl::value_desc("device_filename"));

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

std::unique_ptr<Module> combineModules(std::unique_ptr<Module> destModule, std::unique_ptr<Module> srcModule) {
    Linker::linkModules(*destModule, std::move(srcModule));
    return destModule;
}

void writeToFile(Module &module, const std::string &filename)
{
    std::error_code EC;
    raw_fd_ostream OS(filename, EC);
    if (EC) {
        errs() << "Error opening file: " << EC.message() << "\n";
        return;
    }
    
    module.print(OS, nullptr);
    dbgs() << "Output written to " << filename << "\n";
}
                                          
int main(int argc, char **argv)
{
    InitLLVM x(argc, argv);
    cl::ParseCommandLineOptions(argc, argv, "LLVM Polly Loop Analysis Driver\n");

    dbgs() << "Host IR: " << hostIRPath << "\n";
    dbgs() << "Device IR: " << deviceIRPath << "\n";
    
    LLVMContext context;
    auto deviceModule = loadIRFile(deviceIRPath, context);
    auto hostModule = loadIRFile(hostIRPath, context);
    auto combinedModule = combineModules(CloneModule(*deviceModule), CloneModule(*hostModule));

    DependencyCheckResult result;
    auto analysisPipeline = buildPipeline();
    analysisPipeline.MPM.addPass(LoopFissionDependencyCheckPass(result));

    // // Create a new FunctionPassManager and add it through adaptor properly
    // FunctionPassManager FPM1;
    // FPM1.addPass(polly::ScopAnalysisPrinterPass(dbgs()));
    // analysisPipeline.MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM1)));

    analysisPipeline.MPM.run(*combinedModule, analysisPipeline.MAM);

    auto deviceTransformPipeline = buildPipeline();
    deviceTransformPipeline.MPM.addPass(DeviceKernelLoopFissionPass(result));
    deviceTransformPipeline.MPM.run(*deviceModule, deviceTransformPipeline.MAM);

    auto hostTransformPipeline = buildPipeline();
    hostTransformPipeline.MPM.addPass(HostKernelLoopFissionPass(result));
    hostTransformPipeline.MPM.run(*hostModule, hostTransformPipeline.MAM);

    std::filesystem::path outputPath(hostIRPath.c_str());
    outputPath.remove_filename();
    outputPath /= "output.ll";
    writeToFile(*combinedModule, outputPath.string());

    return 0;
}
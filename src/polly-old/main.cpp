// main.cpp
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "polly/PolyhedralInfo.h"
#include "polly/ScopInfo.h"

#include "polly-old/LoopInfoPass.h"
#include "polly-old/LoopFissionPass.h"
#include "common/helper.h"

#include <fstream>
#include <memory>

#include <assert.h>

using namespace llvm;
using namespace polly;
using namespace std;

extern llvm::cl::opt<bool> PollyCheckParallel;

int main(int argc, char **argv) {
  if (argc < 2) {
    errs() << "Usage: " << argv[0] << " <IR file>\n";
    return 1;
  }
  
  // Turn on polly options
  std::vector<const char*> pollyArgs;
  pollyArgs.push_back(argv[0]);
  pollyArgs.push_back("-polly-process-unprofitable");
  pollyArgs.push_back("-polly-check-parallel"); // must be included in order for isParallel to work
  pollyArgs.push_back("-debug-only=polyhedral-info");
  pollyArgs.push_back("-polly-dependences-computeout=0"); // prevent return invalid independence
  cl::ParseCommandLineOptions(pollyArgs.size(), pollyArgs.data(), "Enable polly options\n");

  llvm::DebugFlag = true;
  llvm::setCurrentDebugType("polyhedral-info");
  
  LLVMContext context;
  
  dbgs() << "Parsing IR file: " << argv[1] << "\n";
  
  SMDiagnostic err;
  std::unique_ptr<Module> program = parseIRFile(argv[1], err, context);
  if (!program) {
    err.print(argv[0], errs());
    return 1;
  }
  
  legacy::FunctionPassManager FPM(program.get());
  
  FPM.add(new PolyhedralInfo());

  FPM.add(new LoopInfoPass());
  
  for (auto &F : *program) {
    if (F.isDeclaration())
      continue;

    // if (!isCudaKernel(&F))
    //   continue;
    dbgs() << "Function: " << F.getName() << "\n";
    FPM.run(F);
  }
  
  dbgs() << "Done\n";
  
  return 0;
}
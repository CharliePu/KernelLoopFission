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

#include "LoopInfoPass.h"
#include "LoopFissionPass.h"

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
    dbgs() << "Function: " << F.getName() << "\n";
    FPM.run(F);
  }
  
  dbgs() << "Done\n";
  
  return 0;
}
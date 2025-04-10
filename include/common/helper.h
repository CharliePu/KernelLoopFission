#ifndef HELPER_H
#define HELPER_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Demangle/Demangle.h"

// Extract just the function name from a demangled signature
inline std::string extractFunctionName(llvm::StringRef mangledName) {
    // Demangle the function name
    std::string demangledName = llvm::demangle(mangledName.str());
    
    // Extract just the function name without parameters
    std::string funcNameOnly;
    size_t paramStart = demangledName.find('(');
    if (paramStart != std::string::npos) {
        // Get everything before the parameter list
        std::string beforeParams = demangledName.substr(0, paramStart);
        
        // Find the last space before parameters (to separate return type)
        size_t lastSpace = beforeParams.rfind(' ');
        if (lastSpace != std::string::npos) {
            // Extract just the function name
            funcNameOnly = beforeParams.substr(lastSpace + 1);
        } else {
            // No space found, use the whole string before parameters
            funcNameOnly = beforeParams;
        }
    } else {
        // No parameters found, use the whole demangled name
        funcNameOnly = demangledName;
    }
    
    return funcNameOnly;
}

// Check if a function is a CUDA kernel
inline bool isCudaKernel(const llvm::Function *F) {
    if (!F) {
        return false; // Cannot check a null function pointer
    }

    // Kernels must belong to a module to have metadata associated
    const llvm::Module *M = F->getParent();
    if (!M) {
        return false; // Function not part of a module
    }

    // Look for the standard named metadata node used by CUDA
    llvm::NamedMDNode *Annotations = M->getNamedMetadata("nvvm.annotations");
    if (!Annotations) {
        // Metadata node not found, likely not a CUDA module or kernel
        return false;
    }

    // Iterate through all annotations in the module
    for (unsigned i = 0, e = Annotations->getNumOperands(); i != e; ++i) {
        llvm::MDNode *AnnotationNode = Annotations->getOperand(i);

        // Each annotation should be an MDNode, typically with 3 operands:
        // {<function ptr>, !"kernel", i32 1}
        if (!AnnotationNode || AnnotationNode->getNumOperands() != 3) {
            continue; // Skip malformed annotation nodes
        }

        // Operand 0: Check if it references our function F
        // It's usually wrapped in ConstantAsMetadata -> ValueAsMetadata
        llvm::Metadata *FuncMDPtr = AnnotationNode->getOperand(0).get();
        if (!FuncMDPtr) continue;

        // Unwrap the metadata to get the potential function pointer
        llvm::Value *FuncVal = nullptr;
        if (auto *VAM = llvm::dyn_cast<llvm::ValueAsMetadata>(FuncMDPtr)) {
             FuncVal = VAM->getValue();
        }
        // Older LLVM/CUDA versions might use ConstantAsMetadata directly
        else if (auto *CAM = llvm::dyn_cast<llvm::ConstantAsMetadata>(FuncMDPtr)) {
             FuncVal = CAM->getValue();
        }


        // Check if the value is indeed our function F
        if (!FuncVal || FuncVal != F) {
            continue; // This annotation is not for our function F
        }

        // Operand 1: Check if it's the MDString "kernel"
        llvm::Metadata *KindMDPtr = AnnotationNode->getOperand(1).get();
        if (!KindMDPtr) continue;

        llvm::MDString *KindStr = llvm::dyn_cast<llvm::MDString>(KindMDPtr);
        if (!KindStr || KindStr->getString() != "kernel") {
            continue; // Not a "kernel" annotation
        }

        // Operand 2: Check if it's the ConstantInt 1
        llvm::Metadata *ValueMDPtr = AnnotationNode->getOperand(2).get();
         if (!ValueMDPtr) continue;

        llvm::ConstantAsMetadata *ValueCAM = llvm::dyn_cast<llvm::ConstantAsMetadata>(ValueMDPtr);
        if (!ValueCAM) continue;

        llvm::ConstantInt *ValueInt = llvm::dyn_cast<llvm::ConstantInt>(ValueCAM->getValue());
        if (!ValueInt || !ValueInt->equalsInt(1)) {
            continue; // Value is not 1
        }

        // If all checks passed, we found the annotation for F identifying it as a kernel
        return true;
    }

    // If we iterated through all annotations and didn't find one for F, it's not a kernel
    return false;
}

// Check if a function is a CUDA kernel device stub
inline bool isCudaKernelDeviceStub(const llvm::Function *F) {
    if (!F) {
        return false; // Cannot check a null function pointer
    }
    
    // Convert function name to string and check if it contains "__device_stub__"
    std::string functionName = extractFunctionName(F->getName());
    return functionName.find("__device_stub__") != std::string::npos;
}

#endif // HELPER_H
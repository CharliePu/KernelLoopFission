#ifndef HELPER_H
#define HELPER_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"

// bool isCudaKernel(const llvm::Function &F)
// {
//     bool callConvention = F.getCallingConv() == llvm::CallingConv::PTX_Kernel;
//     bool metadata = F.getMetadata("nvvm.annotations") != nullptr;
    
//     // Not sure how to check for CUDA kernels
//     return true;
// }

// germini generated
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


#endif // HELPER_H
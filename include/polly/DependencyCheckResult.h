#ifndef LLVM_POLLY_DEPENDENCY_CHECK_RESULT_H
#define LLVM_POLLY_DEPENDENCY_CHECK_RESULT_H

#include <vector>
#include <string>

namespace llvm {

// Stores the results of dependency checking for kernel fission
struct DependencyCheckResult {
    // List of function names that are candidates for kernel fission
    std::vector<std::string> candidateFunctions;
};

} // namespace llvm

#endif // LLVM_POLLY_DEPENDENCY_CHECK_RESULT_H
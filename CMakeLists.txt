cmake_minimum_required(VERSION 3.20.0)
project(KernelLoopFission LANGUAGES CXX C)

# Find Polly (which automatically finds LLVM)
find_package(Polly REQUIRED CONFIG)


set(NO_RTTI "-fno-rtti")
add_definitions(${NO_RTTI})

# Display configuration information
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using PollyConfig.cmake in: ${Polly_CMAKE_DIR}")

# Common include directories
set(COMMON_INCLUDE_DIRS
    ${Polly_INCLUDE_DIRS}  # This already includes LLVM include dirs
    ${CMAKE_SOURCE_DIR}/include
)

# Common compile definitions
set(COMMON_COMPILE_DEFS
    ${Polly_DEFINITIONS}  # This already includes LLVM definitions
)

# Legacy-style Polly-based analysis target (Legacy PM)
add_executable(kernel-loop-fission-polly-old
    src/polly-old/main.cpp
    src/polly-old/LoopInfoPass.cpp
    src/polly-old/LoopFissionPass.cpp
)

target_include_directories(kernel-loop-fission-polly-old PRIVATE ${COMMON_INCLUDE_DIRS})
target_compile_definitions(kernel-loop-fission-polly-old PRIVATE ${COMMON_COMPILE_DEFS})
target_link_libraries(kernel-loop-fission-polly-old PRIVATE Polly)

# New Pass Manager Polly-based analysis target
add_executable(kernel-loop-fission-polly
    src/polly/main.cpp
    src/polly/KernelLoopFissionPass.cpp
    src/polly/LoopFissionDependencyAnalysis.cpp
    src/polly/Pipeline.cpp
    src/polly/LoopFissionDependencyCheckPass.cpp
    src/polly/DeviceKernelLoopFissionPass.cpp
    src/polly/HostKernelLoopFissionPass.cpp
)

target_include_directories(kernel-loop-fission-polly PRIVATE ${COMMON_INCLUDE_DIRS})
target_compile_definitions(kernel-loop-fission-polly PRIVATE ${COMMON_COMPILE_DEFS})
target_link_libraries(kernel-loop-fission-polly PRIVATE Polly)

# Manual Dependency Check Analysis target (Modern PM)
add_executable(kernel-loop-fission-dependency
    src/dependency-check/KernelLoopFissionPass.cpp
    src/dependency-check/LoopFissionDependencyAnalysis.cpp
    src/dependency-check/main.cpp
)

target_include_directories(kernel-loop-fission-dependency PRIVATE ${COMMON_INCLUDE_DIRS})
target_compile_definitions(kernel-loop-fission-dependency PRIVATE ${COMMON_COMPILE_DEFS})
target_link_libraries(kernel-loop-fission-dependency PRIVATE Polly)

# Set output directories
set_target_properties(
    kernel-loop-fission-polly-old
    kernel-loop-fission-polly
    kernel-loop-fission-dependency
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Add test subdirectory
add_subdirectory(tests)
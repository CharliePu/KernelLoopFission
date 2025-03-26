cmake_minimum_required(VERSION 3.20.0)
project(KernelLoopFission LANGUAGES CXX)

# find_package(LLVM REQUIRED CONFIG)
find_package(Polly REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using PollyConfig.cmake in: ${Polly_CMAKE_DIR}")

add_executable(KernelLoopFission src/main.cpp src/LoopInfoPass.cpp)

target_include_directories(KernelLoopFission PRIVATE ${LLVM_INCLUDE_DIRS} ${Polly_INCLUDE_DIRS} include)

target_compile_definitions(KernelLoopFission PRIVATE ${LLVM_DEFINITIONS} ${Polly_DEFINITIONS})

# llvm_map_components_to_libnames(llvm_libs support core irreader analysis passes)
target_link_libraries(KernelLoopFission PRIVATE Polly) #${llvm_libs})
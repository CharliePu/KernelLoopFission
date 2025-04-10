cmake_minimum_required(VERSION 3.20.0)

# Custom function to build CUDA test files to LLVM IR
function(add_test_ir_generation TEST_DIR)
  add_custom_target(${TEST_DIR}_ir
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-ir/${TEST_DIR}
    COMMAND cd ${CMAKE_SOURCE_DIR}/tests/${TEST_DIR} && 
            clang++ --cuda-gpu-arch=sm_89 -O0 main.cu -emit-llvm -S -o ${CMAKE_BINARY_DIR}/test-ir/${TEST_DIR}/main.ll &&
            llvm-dis main-cuda-nvptx64-nvidia-cuda-sm_89.bc -o ${CMAKE_BINARY_DIR}/test-ir/${TEST_DIR}/main_device.ll &&
            rm main-cuda-nvptx64-nvidia-cuda-sm_89.bc
    COMMENT "Generating LLVM IR for ${TEST_DIR} CUDA test"
  )
endfunction()

# Custom function for C++ (non-CUDA) test files
function(add_cpp_test_ir_generation TEST_DIR)
  add_custom_target(${TEST_DIR}_ir
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-ir/${TEST_DIR}
    COMMAND cd ${CMAKE_SOURCE_DIR}/tests/${TEST_DIR} && 
            clang++ main.cpp -O1 -fno-vectorize -fno-slp-vectorize -fno-unroll-loops -emit-llvm -S -o ${CMAKE_BINARY_DIR}/test-ir/${TEST_DIR}/main.ll &&
            opt -passes='loop-simplify,mem2reg' ${CMAKE_BINARY_DIR}/test-ir/${TEST_DIR}/main.ll -S -o ${CMAKE_BINARY_DIR}/test-ir/${TEST_DIR}/main.ll
    COMMENT "Generating LLVM IR for ${TEST_DIR} C++ test"
  )
endfunction()

# Add test targets
add_test_ir_generation(adam-cuda)
add_test_ir_generation(bn-cuda)
add_test_ir_generation(chemv-cuda)
add_test_ir_generation(dense-embedding-cuda)
add_test_ir_generation(f16max-cuda)
add_test_ir_generation(jaccard-cuda)
add_test_ir_generation(mixbench-cuda)
add_test_ir_generation(page-rank-cuda)
add_test_ir_generation(qrg-cuda)
add_test_ir_generation(spm-cuda)
add_test_ir_generation(vote-cuda)

# Add C++ test
add_cpp_test_ir_generation(simple-loops)

# Add a target to build all test IRs
add_custom_target(build_all_tests
  DEPENDS 
    adam-cuda_ir
    bn-cuda_ir
    chemv-cuda_ir
    dense-embedding-cuda_ir
    f16max-cuda_ir
    jaccard-cuda_ir
    mixbench-cuda_ir
    page-rank-cuda_ir
    qrg-cuda_ir
    spm-cuda_ir
    vote-cuda_ir
    simple-loops_ir
)
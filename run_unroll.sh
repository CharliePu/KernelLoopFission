#!/bin/bash

cd ./tests/$1

clang++ --cuda-gpu-arch=sm_89 main.cu -O3 -fno-unroll-loops -emit-llvm -S
# clang++ --cuda-gpu-arch=sm_89 main.cu  -ffinite-loops -O3 -emit-llvm -S
llvm-dis main-cuda-nvptx64-nvidia-cuda-sm_89.bc -o main_device.ll
rm main-cuda-nvptx64-nvidia-cuda-sm_89.bc
# opt -passes="mem2reg,loop-simplify,instcombine,lcssa,indvars" main_device.ll -S -o main_device.opt.ll
opt --polly-canonicalize --polly-print-detect main_device.ll -S -o main_device.ll
../../build/bin/kernel-loop-fission-polly-old main_device.ll
# ../../build/bin/kernel-loop-fission-dependency main_device.ll



# clang++ main.cpp -O3 -fno-vectorize -fno-slp-vectorize -fno-unroll-loops -emit-llvm -S
# # opt --polly --polly-canonicalize --polly-prepare --polly-process-unprofitable --polly-print-detect main.ll -S -o main.ll
# ../../build/bin/kernel-loop-fission-polly-old main.ll
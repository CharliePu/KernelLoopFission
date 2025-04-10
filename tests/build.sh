#!/bin/bash

# DO NOT USE THIS
# USE run.sh INSTEAD

cd $1

# O1 chain
clang++ --cuda-gpu-arch=sm_89 main.cu -O1 -fno-vectorize -fno-slp-vectorize -fno-unroll-loops -emit-llvm -S
llvm-dis main-cuda-nvptx64-nvidia-cuda-sm_89.bc -o main_device.ll
rm main-cuda-nvptx64-nvidia-cuda-sm_89.bc
opt --polly-process-unprofitable -loop-simplify -mem2reg -polly-canonicalize --polly-print-detect --polly-print-dependences main_device.ll -S -o main_device.ll

# # O3 chain
# clang++ --cuda-gpu-arch=sm_89 main.cu -O3 -fno-vectorize -fno-slp-vectorize -fno-unroll-loops -emit-llvm -S
# llvm-dis main-cuda-nvptx64-nvidia-cuda-sm_89.bc -o main_device.ll
# rm main-cuda-nvptx64-nvidia-cuda-sm_89.bc
# opt --polly-process-unprofitable -polly-canonicalize --polly-print-detect --polly-print-dependences main_device.ll -S -o main_device.ll

# # O3 chain
# clang --cuda-gpu-arch=sm_89 main.cu -emit-llvm -S 
# llvm-dis main-cuda-nvptx64-nvidia-cuda-sm_89.bc -o main_device.ll
# rm main-cuda-nvptx64-nvidia-cuda-sm_89.bc main.ll
# opt -O3 main_device.ll -S -o main_device.opt.ll
# opt --polly-canonicalize --polly-print-detect --polly-print-dependences --polly-process-unprofitable main_device.opt.ll -S -o main_device.polly.ll


# clang++ --cuda-gpu-arch=sm_89 -O0 main.cu -emit-llvm -S
# llvm-dis main-cuda-nvptx64-nvidia-cuda-sm_89.bc -o main_device.ll
# rm main-cuda-nvptx64-nvidia-cuda-sm_89.bc
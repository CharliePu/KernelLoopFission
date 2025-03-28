#!/bin/bash

cd $1
clang++ --cuda-gpu-arch=sm_89 -O0 -fno-vectorize -fno-slp-vectorize -fno-unroll-loops  main.cu -emit-llvm -S
llvm-dis main-cuda-nvptx64-nvidia-cuda-sm_89.bc -o main_device.ll
opt -passes='loop-simplify' main_device.ll -S -o main_device.ll
rm main-cuda-nvptx64-nvidia-cuda-sm_89.bc
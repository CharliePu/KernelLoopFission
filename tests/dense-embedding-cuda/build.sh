#!/bin/bash

clang++ --cuda-gpu-arch=sm_89 -O3 main.cu -emit-llvm -S
llvm-dis main-cuda-nvptx64-nvidia-cuda-sm_89.bc
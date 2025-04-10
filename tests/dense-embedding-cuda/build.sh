#!/bin/bash

clang++ --cuda-gpu-arch=sm_89 -O0 main.cu -emit-llvm -S
llvm-dis main-cuda-nvptx64-nvidia-cuda-sm_89.bc -o main_device.ll
rm main-cuda-nvptx64-nvidia-cuda-sm_89.bc
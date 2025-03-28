#!/bin/bash

clang++ main.cpp -O0 -fno-vectorize -fno-slp-vectorize -fno-unroll-loops -emit-llvm -S -o main.ll
opt -passes='loop-simplify' main.ll -S -o main.ll
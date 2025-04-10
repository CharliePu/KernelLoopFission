#!/bin/bash

clang++ main.cpp -O1 -fno-vectorize -fno-slp-vectorize -fno-unroll-loops -emit-llvm -S -o main.ll
opt --polly-process-unprofitable -loop-simplify -mem2reg -polly-canonicalize --polly-print-detect main.ll -S -o main.ll


# opt --polly-process-unprofitable --polly-print-function-dependences --polly-print-detect main.ll
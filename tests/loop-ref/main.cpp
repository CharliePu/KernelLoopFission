// test_dependency.c
// Compile with: clang -O3 -S -emit-llvm test_dependency.c -o test_dependency.ll

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Simple loop with a clear flow dependency
__attribute__((noinline))
void loop_with_dependency(float* __restrict__ A, int N) {
    for (int i = 1; i < N; i++) {
        // Clear dependency: A[i] depends on A[i-1]
        A[i] = A[i-1] + 1.0f;
    }
}

// Loop that mimics your Adam kernel's dependency pattern
__attribute__((noinline))
void adam_like_dependency(float* __restrict__ v, float* __restrict__ m, 
                          float b1, float b2, int N) {
    for (int i = 1; i < N; i++) {
        float grad = i * 0.1f;  // Simulated gradient
        m[i] = b1 * m[i-1] + (1.0f - b1) * grad;
        v[i] = b2 * v[i-1] + (1.0f - b2) * grad * grad;
    }
}

// Two-level nested loop similar to your CUDA kernel
__attribute__((noinline))
void nested_loop_dependency(float* __restrict__ v, float* __restrict__ m, 
                           float* __restrict__ p, float b1, float b2, 
                           int N, int T) {
    for (int i = 1; i < N; i++) {
        for (int t = 0; t < T; t++) {
            float grad = i * 0.1f;
            m[i] = b1 * m[i] + (1.0f - b1) * grad;
            // This is the key dependency
            v[i] = b2 * v[i-1] + (1.0f - b2) * grad * grad;
            p[i] -= 0.01f * (m[i] / sqrtf(v[i]));
        }
    }
}

// A loop that should be identified as parallel
__attribute__((noinline))
void parallel_loop(float* __restrict__ A, float* __restrict__ B, 
                  float* __restrict__ C, int N) {
    for (int i = 0; i < N; i++) {
        C[i] = A[i] + B[i];
    }
}

int main() {
    int N = 1000;
    int T = 10;
    
    float *A = (float*)malloc(N * sizeof(float));
    float *B = (float*)malloc(N * sizeof(float));
    float *C = (float*)malloc(N * sizeof(float));
    float *v = (float*)malloc(N * sizeof(float));
    float *m = (float*)malloc(N * sizeof(float));
    float *p = (float*)malloc(N * sizeof(float));
    
    // Initialize arrays
    for (int i = 0; i < N; i++) {
        A[i] = (float)i;
        B[i] = (float)(i * 2);
        v[i] = 0.1f;
        m[i] = 0.0f;
        p[i] = 1.0f;
    }
    
    // Call the test functions
    loop_with_dependency(A, N);
    adam_like_dependency(v, m, 0.9f, 0.999f, N);
    nested_loop_dependency(v, m, p, 0.9f, 0.999f, N, T);
    parallel_loop(A, B, C, N);
    
    // Print some results to prevent optimization from removing the calls
    printf("A[500] = %f\n", A[500]);
    printf("v[500] = %f\n", v[500]);
    printf("C[500] = %f\n", C[500]);
    
    // Free memory
    free(A);
    free(B);
    free(C);
    free(v);
    free(m);
    free(p);
    
    return 0;
}
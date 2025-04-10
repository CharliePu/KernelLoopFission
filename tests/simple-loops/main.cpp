#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void loop(float* A, float *B, int N) {
    for (int i = 1; i < N; i++) {
        B[i] = A[i] + 1.0;
    }
}

void loop_with_dependency(float* A, float* B, int N) {
    for (int i = 1; i < N; i++) {
        B[i] = A[i] + B[i-1];
    }
}

void nested_loop(float* A, float* B, float* C, int N) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            C[i+j*N] = B[i+j] * A[i*N+j];
        }
    }
}

extern int get_batch_index();
extern int get_thread_block_size();
extern int get_thread_id();
extern int __any_sync(int mask, int predicate);
extern void __syncthreads();

void dense_esuhm2(
    const float* input,
    const float* dense,
          float* output,
    const unsigned int embedding_dim,
    const int* offset)
{
  // for (int bid = 0; bid < 1000; bid++) {
  //   for (int tid = 0; tid < 1000; tid++) {
  //     const int batch_idx = bid;
  //     const int grain_size = 1000;

      const int batch_idx = 10;  // replaces blockIdx.x
      const int grain_size = 20;  // replaces blockDim.x
      const int tid = 30;  // replaces threadIdx.x

      const int range = offset[batch_idx + 1] - offset[batch_idx];
      for (int idx = tid; idx < embedding_dim; idx += grain_size) {
        const float dense_elem = dense[batch_idx * embedding_dim + idx];
        for (int nested_idx = idx; nested_idx < range; nested_idx += 10) {
          output[offset[batch_idx] + nested_idx] = input[offset[batch_idx] + nested_idx] + dense_elem;
        }
      }
    }
  // }
// }

void dense_esuhm3(
  const float* input,
  const float* dense,
        float* output,
  int embedding_dim,
  const int* offset)
{
const int batch_idx = get_batch_index();  // replaces blockIdx.x
const int grain_size = get_thread_block_size();  // replaces blockDim.x
const int tid = get_thread_id();  // replaces threadIdx.x

const int range = offset[batch_idx + 1] - offset[batch_idx];
for (int idx = tid; idx < embedding_dim; idx += grain_size) {
  const float dense_elem = dense[batch_idx * embedding_dim + idx];
  for (int nested_idx = idx; nested_idx < range; nested_idx += embedding_dim) {
    output[offset[batch_idx] + nested_idx] = input[offset[batch_idx] + nested_idx] + dense_elem;
  }
}
}

void VoteAnyKernel1(float *input, float *result,
  int repeat) {
  int tx = get_thread_id();  // replaces threadIdx.x
  for (int i = 0; i < repeat; i++)
  // result[tx] = __any_sync(-1, input[tx]);
  result[tx] = __any_sync(-1, input[tx]);

}

// void dense_esuhm3(
//   const float* input,
//   const float* dense,
//         float* output,
//   int embedding_dim,
//   const int* offset)
// {
// const int batch_idx = get_batch_index();  // replaces blockIdx.x
// const int grain_size = get_thread_block_size();  // replaces blockDim.x
// const int tid = get_thread_id();  // replaces threadIdx.x

// const int range = offset[batch_idx + 1] - offset[batch_idx];
// for (int idx = tid; idx < embedding_dim; idx += grain_size) {
//   const float dense_elem = dense[batch_idx * embedding_dim + idx];
//   for (int nested_idx = idx; nested_idx < range; nested_idx += embedding_dim) {
//     output[offset[batch_idx] + nested_idx] = input[offset[batch_idx] + nested_idx] + dense_elem;
//   }
// }
// }

int main() {
    int N = 1000;
    int T = 10;
    
    float *A = (float*)malloc(N * sizeof(float));
    float *B = (float*)malloc(N * sizeof(float));
    float *C = (float*)malloc(N * sizeof(float));
    float *v = (float*)malloc(N * sizeof(float));
    int *D = (int*)malloc(N * sizeof(int));
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
    
    loop(A, B, N);
    loop_with_dependency(A, B, N);
    nested_loop(A, B, C, N);

    dense_esuhm2(A, B, C, N, D);

    dense_esuhm3(A, B, C, N, D);

    VoteAnyKernel1(A, B, 300);

    
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
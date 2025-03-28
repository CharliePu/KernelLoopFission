#include <iostream>
#include <vector>
#include <cmath> // For fabs in verification

// Define loop bounds. Using #define or const int makes it clearer
// for the compiler (and Polly) that these are parameters constant
// during the loop execution.
#define N 1024
#define M 1024

// Use statically allocated arrays for simplicity in this example.
// Polly can also work with dynamically allocated memory, but requires
// alias analysis to be confident. Static allocation simplifies this.
double A[N][M];
double B[N][M];
double C[N][M];

// Function to initialize arrays
void initialize_arrays() {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < M; ++j) {
            B[i][j] = static_cast<double>(i) * 0.5 + static_cast<double>(j) * 0.3;
            C[i][j] = static_cast<double>(i) * 0.2 + static_cast<double>(j) * 0.7;
            A[i][j] = 0.0; // Initialize output array
        }
    }
}

// Function containing the target loop nest for Polly
// Added 'noinline' attribute to make it easier to isolate in analysis/profiling
__attribute__((noinline))
void compute() {
    // --- SCoP Begin ---
    // Polly should identify this nested loop as a SCoP.
    // The operations are simple, accesses and bounds are affine.
    // Iterations are independent, making it suitable for parallelization.
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < M; ++j) {
            A[i][j] = B[i][j] * 3.14 + C[i][j] * 1.618;
        }
    }
    // --- SCoP End ---
}


int main() {
    initialize_arrays();

    compute();

    // Optional: Verification or use of the result to prevent dead code elimination.
    // If the compiler sees the result isn't used, it might remove the loop entirely,
    // preventing Polly from ever seeing it.
    double check_sum = 0.0;
    // Using volatile avoids optimization but adds overhead.
    // A simpler way is just to print or return a result.
    for (int i = 0; i < N; i += N/10) { // Check only a few elements
        for (int j = 0; j < M; j += M/10) {
            check_sum += A[i][j];
        }
    }

    std::cout << "Verification checksum: " << check_sum << std::endl;
    std::cout << "Expected value for A[N/2][M/2]: "
              << (B[N/2][M/2] * 3.14 + C[N/2][M/2] * 1.618) << std::endl;
    std::cout << "Actual value for A[N/2][M/2]:   "
              << A[N/2][M/2] << std::endl;

     if (std::fabs(A[N/2][M/2] - (B[N/2][M/2] * 3.14 + C[N/2][M/2] * 1.618)) > 1e-9) {
         std::cerr << "Verification FAILED!" << std::endl;
         return 1;
     } else {
          std::cout << "Verification PASSED (for checked element)." << std::endl;
     }


    return 0;
}
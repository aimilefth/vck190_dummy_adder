//===------------------------------------------------------------*- C++ -*-===//
// tb_gemm.cpp
//
// Testbench for HLS gemm (Element-wise addition implementation)
// Generates input data internally instead of reading .bin files.
//===----------------------------------------------------------------------===//

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <iomanip>

// Include HLS headers
#include "gemm_common.h"

// For data generation randomization
#include <random>

using namespace std;

// Configuration
bool PRINT_MISSMATCH = true;
// Tolerance for float comparison
const float EPSILON = 0.001f; 

// Helper to generate random float data
void generate_data(float* data, size_t num_elements) {
    for (size_t i = 0; i < num_elements; i++) {
        // Generate random float between 0.0 and 10.0
        data[i] = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 10.0f));
    }
}

// Helper to calculate Golden Reference
// NOTE: Based on the 'adder' function in gemm.cpp, this kernel performs
// element-wise addition: C[i] = A[i] + B[i]
void compute_golden(float* A, float* B, float* C_golden, int M, int N) {
    for (int i = 0; i < M * N; i++) {
        C_golden[i] = A[i] + B[i];
    }
}

int main(int argc, char **argv) {
    std::cout << "Starting Testbench..." << std::endl;
    std::cout << "Matrix Dimensions: " << GEMM_M << "x" << GEMM_N << std::endl;

    // Calculate total elements
    const size_t A_ELEMS = (size_t)GEMM_M * (size_t)GEMM_K;
    const size_t B_ELEMS = (size_t)GEMM_K * (size_t)GEMM_N;
    const size_t C_ELEMS = (size_t)GEMM_M * (size_t)GEMM_N;

    // Using std::vector to ensure heap allocation (prevents stack overflow on large matrices)
    // We treat them as flat 1D arrays which matches the memory layout of 2D C-arrays
    std::vector<float> input_A(A_ELEMS);
    std::vector<float> input_B(B_ELEMS);
    std::vector<float> output_C_hw(C_ELEMS); // Hardware Output
    std::vector<float> output_C_sw(C_ELEMS); // Software Golden Reference

    // 1. Generate Input Data
    std::cout << "Generating input data..." << std::endl;
    generate_data(input_A.data(), A_ELEMS);
    generate_data(input_B.data(), B_ELEMS);

    // Initialize output to zero
    std::fill(output_C_hw.begin(), output_C_hw.end(), 0.0f);

    // 2. Run Golden Reference (Software)
    std::cout << "Calculating golden reference (Addition)..." << std::endl;
    // Note: Assuming sizes allow element-wise addition (M*K == K*N in this config)
    compute_golden(input_A.data(), input_B.data(), output_C_sw.data(), GEMM_M, GEMM_N);

    // 3. Run HLS Kernel
    std::cout << "Calling HLS Kernel..." << std::endl;
    // Cast flat float pointers to float16* as expected by the kernel
    gemm((float16*)input_A.data(), (float16*)input_B.data(), (float16*)output_C_hw.data());

    // 4. Verify Results
    std::cout << "Verifying results..." << std::endl;
    int mismatch_count = 0;
    bool passed = true;

    for (int i = 0; i < GEMM_M; i++) {
        for (int j = 0; j < GEMM_N; j++) {
            // Map 2D indices to flat 1D index
            int idx = i * GEMM_N + j;

            float hw_val = output_C_hw[idx];
            float sw_val = output_C_sw[idx];
            float diff = fabsf(hw_val - sw_val);

            if (diff > EPSILON) {
                passed = false;
                mismatch_count++;
                if (PRINT_MISSMATCH && mismatch_count < 10) { // Limit print output
                    std::cout << "Mismatch at [" << i << "][" << j << "]: "
                              << "HW=" << hw_val << " "
                              << "SW=" << sw_val << " "
                              << "Diff=" << diff << "\n";
                }
            }
        }
    }

    std::cout << "Total mismatches: " << mismatch_count << "\n";

    // 5. Debug Print (Last Row)
    std::cout << "\n=== Debug: last row (index " << (GEMM_M-1) << ") ===\n";
    
    std::cout << "HW Output (Partial):\n";
    for (int j = 0; j < 8; j++) std::cout << output_C_hw[(GEMM_M-1)*GEMM_N + j] << ", ";
    std::cout << "...\n";

    std::cout << "SW Golden (Partial):\n";
    for (int j = 0; j < 8; j++) std::cout << output_C_sw[(GEMM_M-1)*GEMM_N + j] << ", ";
    std::cout << "...\n";

    if (passed) {
        std::cout << "\n*** TEST PASSED ***\n" << std::endl;
        return 0;
    } else {
        std::cout << "\n!!! TEST FAILED !!!\n" << std::endl;
        return 1;
    }
}
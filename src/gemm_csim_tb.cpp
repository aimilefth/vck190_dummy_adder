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

// A + B + C
void compute_golden(float* A, float* B, float* C_in, float* D_golden, int M, int N) {
    for (int i = 0; i < M * N; i++) {
        D_golden[i] = A[i] + B[i] + C_in[i];
    }
}

int main(int argc, char **argv) {
    std::cout << "Starting Testbench (A+B+C)..." << std::endl;
    
    const size_t A_ELEMS = (size_t)GEMM_M * (size_t)GEMM_K;
    const size_t B_ELEMS = (size_t)GEMM_K * (size_t)GEMM_N;
    const size_t C_ELEMS = (size_t)GEMM_M * (size_t)GEMM_N;
    const size_t D_ELEMS = (size_t)GEMM_M * (size_t)GEMM_N;

    std::vector<float> input_A(A_ELEMS);
    std::vector<float> input_B(B_ELEMS);
    std::vector<float> input_C(C_ELEMS); // NEW
    std::vector<float> output_D_hw(D_ELEMS);
    std::vector<float> output_D_sw(D_ELEMS);

    generate_data(input_A.data(), A_ELEMS);
    generate_data(input_B.data(), B_ELEMS);
    generate_data(input_C.data(), C_ELEMS);

    compute_golden(input_A.data(), input_B.data(), input_C.data(), output_D_sw.data(), GEMM_M, GEMM_N);

    std::cout << "Calling HLS Kernel..." << std::endl;
    // 4 Arguments now
    gemm((float16*)input_A.data(), (float16*)input_B.data(), (float16*)input_C.data(), (float16*)output_D_hw.data());

    int mismatch_count = 0;
    for (int i = 0; i < GEMM_M * GEMM_N; i++) {
        float diff = fabsf(output_D_hw[i] - output_D_sw[i]);
        if (diff > EPSILON) {
            mismatch_count++;
            if (PRINT_MISSMATCH && mismatch_count < 5) std::cout << "Mismatch " << diff << "\n";
        }
    }

    if (mismatch_count == 0) std::cout << "\n*** TEST PASSED ***\n";
    else std::cout << "\n!!! TEST FAILED !!!\n";

    return (mismatch_count == 0) ? 0 : 1;
}
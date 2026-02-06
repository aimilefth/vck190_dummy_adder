#include "host_gemm_fpga.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib> // for rand
#include <ctime>   // for time

// Helper to fill buffer with random floats
static void generate_random_data(float* dst, std::size_t n_floats) {
    for (std::size_t i = 0; i < n_floats; i++) {
        // Generate random float between 0.0 and 10.0
        dst[i] = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 10.0f));
    }
}

// Helper to calculate Software Reference
// NOTE: Based on your HLS kernel 'adder', this performs Element-wise Addition.
// If your kernel performs actual GEMM (Matrix Mult), change this logic.
static void compute_golden(const float* A, const float* B, std::vector<float>& C, int M, int N) {
    std::size_t total_elements = (std::size_t)M * (std::size_t)N;
    C.resize(total_elements);
    
    for (std::size_t i = 0; i < total_elements; ++i) {
        C[i] = A[i] + B[i];
    }
}

int main(int argc, char** argv) {
    // Seed random number generator
    srand((unsigned)time(NULL));

    if (argc < 2) {
        std::cerr << "Usage:\n  " << argv[0] << " <xclbin_path> [device_index] [iterations]\n";
        return 1;
    }

    const std::string xclbin_path = argv[1];
    const unsigned device_index = (argc >= 3) ? (unsigned)std::stoul(argv[2]) : 0;
    const unsigned iterations  = (argc >= 4) ? (unsigned)std::stoul(argv[3]) : 1;

    std::cout << "Initializing FPGA...\n";
    
    FPGA_GEMM fpga;
    if (fpga.fpga_init(xclbin_path, device_index) != 0) {
        std::cerr << "FPGA init failed.\n";
        return 1;
    }

    std::cout << "Generating random input data...\n";

    // 1. Generate Input Data directly into mapped pointers
    // Note: We use the sizes defined in the class (which usually come from host_visible.h)
    generate_random_data(fpga.get_inA_ptr(), FPGA_GEMM::A_ELEMS);
    generate_random_data(fpga.get_inB_ptr(), FPGA_GEMM::B_ELEMS);

    // 2. Compute Golden Reference (Software)
    std::cout << "Computing golden reference (Software)...\n";
    std::vector<float> golden;
    compute_golden(fpga.get_inA_ptr(), fpga.get_inB_ptr(), golden, GEMM_M, GEMM_N);

    // 3. Optional Warmup
    std::cout << "Warming up...\n";
    fpga.warmup(1);

    // 4. Run Hardware
    std::cout << "Running FPGA Kernel (" << iterations << " iterations)...\n";
    for (unsigned i = 0; i < iterations; ++i) {
        fpga.run();
    }

    // 5. Compare Results
    std::cout << "Verifying results...\n";
    bool pass = true;
    const float tol = 0.1f;
    float* outC = fpga.get_outC_ptr();

    for (int r = 0; r < GEMM_M; ++r) {
        for (int c = 0; c < GEMM_N; ++c) {
            const std::size_t idx = (std::size_t)r * (std::size_t)GEMM_N + (std::size_t)c;
            const float diff = std::fabs(outC[idx] - golden[idx]);
            if (diff > tol) {
                pass = false;
                // Optional: Print first few failures
                // std::cerr << "Mismatch at " << r << "," << c << " HW: " << outC[idx] << " SW: " << golden[idx] << "\n";
            }
        }
    }

    // Debug print: last row (Index GEMM_M-1)
    std::cout << "\n=== Debug: last row (index " << (GEMM_M - 1) << ") ===\n";
    
    std::cout << "outC[GEMM_M-1][*] (computed):\n";
    for (int j = 0; j < GEMM_N; j++) {
        std::cout << outC[(std::size_t)(GEMM_M - 1) * GEMM_N + j];
        if (j != GEMM_N - 1) std::cout << ", ";
    }

    std::cout << "\n\ngolden[GEMM_M-1][*] (reference):\n";
    for (int j = 0; j < GEMM_N; j++) {
        std::cout << golden[(std::size_t)(GEMM_M - 1) * GEMM_N + j];
        if (j != GEMM_N - 1) std::cout << ", ";
    }

    std::cout << "\n\nabs(outC - golden) row GEMM_M-1:\n";
    for (int j = 0; j < GEMM_N; j++) {
        float d = std::fabs(outC[(std::size_t)(GEMM_M - 1) * GEMM_N + j] -
                            golden[(std::size_t)(GEMM_M - 1) * GEMM_N + j]);
        std::cout << d;
        if (j != GEMM_N - 1) std::cout << ", ";
    }
    std::cout << "\n=================================\n\n";

    fpga.print_performance_timings();

    if (!pass) {
        std::cerr << "FAILED (tol=" << tol << ")\n";
        return 1;
    }

    std::cout << "PASSED\n";
    return 0;
}
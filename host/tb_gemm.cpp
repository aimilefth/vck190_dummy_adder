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

// Helper to calculate Software Reference (A + B + C)
static void compute_golden(const float* A, const float* B, const float* C, const float* D, std::vector<float>& E, int M, int N) {
    std::size_t total_elements = (std::size_t)M * (std::size_t)N;
    E.resize(total_elements);
    
    for (std::size_t i = 0; i < total_elements; ++i) {
        E[i] = A[i] + B[i] + C[i] + D[i]; // Added C
    }
}

int main(int argc, char** argv) {
    // Seed random number generator
    srand((unsigned)time(NULL));

    if (argc < 2) {
        std::cerr << "Usage:\n  " << argv[0] << " <xclbin_path> [device_index] [iterations]" << std::endl;
        return 1;
    }

    const std::string xclbin_path = argv[1];
    const unsigned device_index = (argc >= 3) ? (unsigned)std::stoul(argv[2]) : 0;
    const unsigned iterations  = (argc >= 4) ? (unsigned)std::stoul(argv[3]) : 1;

    std::cout << "Initializing FPGA..." << std::endl;
    
    FPGA_GEMM fpga;
    if (fpga.fpga_init(xclbin_path, device_index) != 0) {
        std::cerr << "FPGA init failed." << std::endl;
        return 1;
    }

    std::cout << "Generating random input data..." << std::endl;

    // 1. Generate Input Data directly into mapped pointers
    // Note: We use the sizes defined in the class (which usually come from host_visible.h)
    generate_random_data(fpga.get_inA_ptr(), FPGA_GEMM::A_ELEMS);
    generate_random_data(fpga.get_inB_ptr(), FPGA_GEMM::B_ELEMS);
    generate_random_data(fpga.get_inC_ptr(), FPGA_GEMM::C_ELEMS);
    generate_random_data(fpga.get_inD_ptr(), FPGA_GEMM::D_ELEMS);

    // 2. Compute Golden Reference (Software)
    std::cout << "Computing golden reference (Software A+B+C)..." << std::endl;
    std::vector<float> golden;
    compute_golden(fpga.get_inA_ptr(), fpga.get_inB_ptr(), fpga.get_inC_ptr(),  fpga.get_inD_ptr(), golden, GEMM_M, GEMM_N);

    std::cout << "Warming up..." << std::endl;
    fpga.warmup(1);

    std::cout << "Running FPGA Kernel (" << iterations << " iterations)..." << std::endl;
    for (unsigned i = 0; i < iterations; ++i) {
        fpga.run();
    }

    std::cout << "Verifying results..." << std::endl;
    bool pass = true;
    const float tol = 0.1f;
    float* outE = fpga.get_outE_ptr();

    for (int r = 0; r < GEMM_M; ++r) {
        for (int c = 0; c < GEMM_N; ++c) {
            const std::size_t idx = (std::size_t)r * (std::size_t)GEMM_N + (std::size_t)c;
            const float diff = std::fabs(outE[idx] - golden[idx]);
            if (diff > tol) pass = false;
        }
    }

    // Debug print
    std::cout << "\n=== Debug: last row ===" << std::endl;
    std::cout << "outE (HW): ";
    for (int j = 0; j < 8; j++) std::cout << outE[(std::size_t)(GEMM_M - 1) * GEMM_N + j] << ", ";
    std::cout << "\ngolden (SW): ";
    for (int j = 0; j < 8; j++) std::cout << golden[(std::size_t)(GEMM_M - 1) * GEMM_N + j] << ", ";
    std::cout << "\n";

    fpga.print_performance_timings();

    if (!pass) {
        std::cerr << "FAILED (tol=" << tol << ")" << std::endl;
        return 1;
    }

    std::cout << "PASSED" << std::endl;
    return 0;
}
#ifndef HOST_GEMM_FPGA_H__
#define HOST_GEMM_FPGA_H__

// Matrix dims (host-visible)
#include "src/host_visible.h"

#include <xrt/xrt_device.h>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_kernel.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

class FPGA_GEMM {
public:
    int fpga_init(const std::string& xclbin_path, unsigned int device_index = 0);
    void warmup(unsigned int iterations = 1);
    void run();
    void run_benchmark(unsigned int iterations);
    void run_benchmark_hostdatatransfer(unsigned int iterations);
    void print_performance_timings() const;
    float* get_inA_ptr();
    float* get_inB_ptr();
    float* get_outC_ptr();

    // Sizes (in floats)
    static constexpr std::size_t A_ELEMS = (std::size_t)GEMM_M * (std::size_t)GEMM_K;
    static constexpr std::size_t B_ELEMS = (std::size_t)GEMM_K * (std::size_t)GEMM_N;
    static constexpr std::size_t C_ELEMS = (std::size_t)GEMM_M * (std::size_t)GEMM_N;

private:
    xrt::device device;
    xrt::kernel gemm_kernel;

    xrt::bo inA_bo;
    xrt::bo inB_bo;
    xrt::bo outC_bo;

    float* inA_host_ptr;
    float* inB_host_ptr;
    float* outC_host_ptr;

    xrt::run run_gemm;

    // Timing data
    std::chrono::duration<double, std::micro> time_program_fpga;
    std::chrono::duration<double, std::micro> time_allocate_buffers;
    std::vector<std::chrono::duration<double, std::micro>> warmup_timings;
    std::vector<std::chrono::duration<double, std::micro>> time_copy_input_to_device;
    std::vector<std::chrono::duration<double, std::micro>> time_kernel_execution;
    std::vector<std::chrono::duration<double, std::micro>> time_copy_output_to_host;
};

#endif // HOST_GEMM_FPGA_H__

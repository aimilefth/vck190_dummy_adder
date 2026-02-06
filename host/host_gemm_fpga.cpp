#include "host_gemm_fpga.h"

#include <experimental/xrt_xclbin.h>
#include <iostream>


int FPGA_GEMM::fpga_init(const std::string& xclbin_path, const unsigned int device_index) {
    using clock = std::chrono::high_resolution_clock;
    auto start = clock::now();
    device = xrt::device(device_index);

    std::cout << "Trying to program device[" << device_index << "] with name: "
              << device.get_info<xrt::info::device::name>()
              << " and bdf: " << device.get_info<xrt::info::device::bdf>()
              << std::endl;

    auto uuid = device.load_xclbin(xclbin_path);

    // Kernel name must match the HLS top function: "gemm"
    gemm_kernel = xrt::kernel(device, uuid, "gemm");

    auto end_program = clock::now();
    time_program_fpga = end_program - start;

    std::cout << "Device[" << device_index << "]: programmed successfully. Kernel handle: "
              << gemm_kernel.get_handle() << std::endl;

    std::cout << "Allocate buffers in global memory" << std::endl;
    auto start_alloc = clock::now();

    const std::size_t A_BYTES = A_ELEMS * sizeof(float);
    const std::size_t B_BYTES = B_ELEMS * sizeof(float);
    const std::size_t C_BYTES = C_ELEMS * sizeof(float);
    const std::size_t D_BYTES = D_ELEMS * sizeof(float);

    // Map to memory banks. Trying to distribute inputs.
    // A->Grp0, B->Grp1, C->Grp2 (if avail) or share, D->Grp0
    inA_bo  = xrt::bo(device, A_BYTES, gemm_kernel.group_id(0));
    inB_bo  = xrt::bo(device, B_BYTES, gemm_kernel.group_id(1));
    inC_bo  = xrt::bo(device, C_BYTES, gemm_kernel.group_id(2)); // NEW Arg 2
    outD_bo = xrt::bo(device, D_BYTES, gemm_kernel.group_id(3)); // Arg 3

    inA_host_ptr  = inA_bo.map<float*>();
    inB_host_ptr  = inB_bo.map<float*>();
    inC_host_ptr  = inC_bo.map<float*>();
    outD_host_ptr = outD_bo.map<float*>();

    std::cout << "Mapped pointers: A=" << (void*)inA_host_ptr
            << " B=" << (void*)inB_host_ptr
            << " C=" << (void*)inC_host_ptr 
            << " D=" << (void*)outD_host_ptr << "\n";

    if (inA_host_ptr == nullptr || inB_host_ptr == nullptr || inC_host_ptr == nullptr || outD_host_ptr == nullptr) {
        std::cerr << "Error: Failed to map XRT Buffer Objects to host memory!" << std::endl;
        return -1;
    }

    run_gemm = xrt::run(gemm_kernel);
    run_gemm.set_arg(0, inA_bo);
    run_gemm.set_arg(1, inB_bo);
    run_gemm.set_arg(2, inC_bo);  // NEW
    run_gemm.set_arg(3, outD_bo); // WAS 2

    auto end_alloc = clock::now();
    time_allocate_buffers = end_alloc - start_alloc;

    return 0;
}

void FPGA_GEMM::warmup(unsigned int iterations) {
    using clock = std::chrono::high_resolution_clock;
    warmup_timings.clear();
    warmup_timings.reserve(iterations);

    for (unsigned int i = 0; i < iterations; ++i) {
        auto start = clock::now();
        run_gemm.start();
        run_gemm.wait();
        auto end = clock::now();
        warmup_timings.push_back(end - start);
        std::cout << "Warmed up (" << (i + 1) << "/" << iterations << ")\n";
    }
}

void FPGA_GEMM::run() {
    using clock = std::chrono::high_resolution_clock;
    // Copy inputs to device
    auto start_copy_in = clock::now();
    inA_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    inB_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    inC_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE); // NEW
    auto end_copy_in = clock::now();
    time_copy_input_to_device.push_back(end_copy_in - start_copy_in);

    // Execute kernel
    auto start_kernel = clock::now();
    run_gemm.start();
    run_gemm.wait();
    auto end_kernel = clock::now();
    time_kernel_execution.push_back(end_kernel - start_kernel);

    // Copy output to host
    auto start_copy_out = clock::now();
    outD_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    auto end_copy_out = clock::now();
    time_copy_output_to_host.push_back(end_copy_out - start_copy_out);
}

void FPGA_GEMM::run_benchmark(unsigned int iterations) {
    using clock = std::chrono::high_resolution_clock;
    std::vector<double> kernel_ms;
    kernel_ms.reserve(iterations);

    for (unsigned int i = 0; i < iterations; ++i) {
        auto start = clock::now();
        run_gemm.start();
        run_gemm.wait();
        auto end = clock::now();
        kernel_ms.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }

    double sum = 0.0;
    for (auto v : kernel_ms) sum += v;
    std::cout << "Kernel-only benchmark over " << iterations
              << " iters, avg: " << (sum / iterations) << " ms\n";
}

void FPGA_GEMM::run_benchmark_hostdatatransfer(unsigned int iterations) {
    using clock = std::chrono::high_resolution_clock;
    std::vector<double> copy_in_ms, kernel_ms, copy_out_ms, total_ms;
    copy_in_ms.reserve(iterations);
    kernel_ms.reserve(iterations);
    copy_out_ms.reserve(iterations);
    total_ms.reserve(iterations);

    for (unsigned int i = 0; i < iterations; ++i) {
        auto start_total = clock::now();

        auto start_ci = clock::now();
        inA_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        inB_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        inC_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE); // NEW
        auto end_ci = clock::now();
        copy_in_ms.push_back(std::chrono::duration<double, std::milli>(end_ci - start_ci).count());

        auto start_k = clock::now();
        run_gemm.start();
        run_gemm.wait();
        auto end_k = clock::now();
        kernel_ms.push_back(std::chrono::duration<double, std::milli>(end_k - start_k).count());

        auto start_co = clock::now();
        outD_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        auto end_co = clock::now();
        copy_out_ms.push_back(std::chrono::duration<double, std::milli>(end_co - start_co).count());

        auto end_total = clock::now();
        total_ms.push_back(std::chrono::duration<double, std::milli>(end_total - start_total).count());
    }

    // Compute average times
    double total_copy_in_ms = 0;
    double total_kernel_ms = 0;
    double total_copy_out_ms = 0;
    double total_total_ms = 0;

    for (unsigned int i = 0; i < iterations; ++i) {
        total_copy_in_ms += copy_in_ms[i];
        total_kernel_ms += kernel_ms[i];
        total_copy_out_ms += copy_out_ms[i];
        total_total_ms += total_ms[i];
    }

    double avg_copy_in_ms = total_copy_in_ms / iterations;
    double avg_kernel_ms = total_kernel_ms / iterations;
    double avg_copy_out_ms = total_copy_out_ms / iterations;
    double avg_total_ms = total_total_ms / iterations;

    std::cout << "Benchmarking with data transfers over " << iterations << " iterations." << std::endl;
    std::cout << "Average Input Transfer ms: " << avg_copy_in_ms << " ms" << std::endl;
    std::cout << "Average Kernel Execution ms: " << avg_kernel_ms << " ms" << std::endl;
    std::cout << "Average Output Transfer ms: " << avg_copy_out_ms << " ms" << std::endl;
    std::cout << "Average Total ms: " << avg_total_ms << " ms" << std::endl;
}

void FPGA_GEMM::print_performance_timings() const {
    std::cout << "FPGA Initialization Timings:\n";
    std::cout << "  Time to program FPGA: " << time_program_fpga.count() << " us\n";
    std::cout << "  Time to allocate buffers: " << time_allocate_buffers.count() << " us\n";

    if (!warmup_timings.empty()) {
        double sum = 0.0;
        for (auto& t : warmup_timings) sum += t.count();
        std::cout << "Warmup Timings:\n";
        std::cout << "  iters: " << warmup_timings.size()
                  << ", avg: " << (sum / warmup_timings.size()) << " us\n";
    }

    std::cout << "Run Timings:\n";
    for (std::size_t i = 0; i < time_kernel_execution.size(); ++i) {
        std::cout << "  Run " << (i + 1) << ":\n";
        std::cout << "    copy-in : " << time_copy_input_to_device[i].count() << " us\n";
        std::cout << "    kernel  : " << time_kernel_execution[i].count() << " us\n";
        std::cout << "    copy-out: " << time_copy_output_to_host[i].count() << " us\n";
    }
}

float* FPGA_GEMM::get_inA_ptr() { return inA_host_ptr; }
float* FPGA_GEMM::get_inB_ptr() { return inB_host_ptr; }
float* FPGA_GEMM::get_inC_ptr() { return inC_host_ptr; }
float* FPGA_GEMM::get_outD_ptr() { return outD_host_ptr; }
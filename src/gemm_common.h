// gemm_common.h
#ifndef GEMM_COMMON_H
#define GEMM_COMMON_H

#include <hls_stream.h>
#include <ap_int.h>
#include <hls_vector.h>
#include "host_visible.h"

// Calculate total vector chunks needed
// Assuming GEMM_M*GEMM_K and GEMM_K*GEMM_N and GEMM_M*GEMM_N are multiples of 16
#define num_chunks_A (GEMM_M * GEMM_K) / 16
#define num_chunks_B (GEMM_K * GEMM_N) / 16
#define num_chunks_C (GEMM_M * GEMM_N) / 16

#define TILE_M 1
#define TILE_N 1

// Define Vector width for 512-bit AXI (16 * 32-bit float = 512 bits)
typedef hls::vector<float, 16> float16;

void gemm(float16 inA[num_chunks_A], float16 inB[num_chunks_B], float16 outC[num_chunks_C]);

#endif //GEMM_COMMON_H
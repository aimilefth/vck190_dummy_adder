#ifndef GEMM_COMMON_H
#define GEMM_COMMON_H

#include <hls_stream.h>
#include <ap_int.h>
#include <hls_vector.h>
#include "host_visible.h"

// Calculate total vector chunks needed
#define num_chunks_A (GEMM_M * GEMM_K) / 16
#define num_chunks_B (GEMM_K * GEMM_N) / 16
#define num_chunks_C (GEMM_M * GEMM_N) / 16
#define num_chunks_D (GEMM_M * GEMM_N) / 16 // NEW Output

#define TILE_M 1
#define TILE_N 1

typedef hls::vector<float, 16> float16;

// Updated signature for 3 inputs, 1 output
void gemm(float16 inA[num_chunks_A], float16 inB[num_chunks_B], float16 inC[num_chunks_C], float16 outD[num_chunks_D]);

#endif //GEMM_COMMON_H
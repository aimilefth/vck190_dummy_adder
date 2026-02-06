//src/communication.hpp
// Here exist all the function related to DDR/stream and controller logic
#include "gemm_common.h"
#include "utils/x_hls_utils.h"


void load_input_A(const float16* inA, hls::stream<float16>& a_stream) {
    #pragma HLS INLINE off
    load_A: for (int i = 0; i < num_chunks_A; i++) {
        #pragma HLS PIPELINE II=1
        a_stream.write(inA[i]);
    }
}

void load_input_B(const float16* inB, hls::stream<float16>& b_stream) {
    #pragma HLS INLINE off
    load_B: for (int i = 0; i < num_chunks_B; i++) {
        #pragma HLS PIPELINE II=1
        b_stream.write(inB[i]);
    }
}


void load_input(
    const float16* inA,
    const float16* inB,
    hls::stream<float16>& a_stream,
    hls::stream<float16>& b_stream
) {
    #pragma HLS INLINE off
    #pragma HLS DATAFLOW
    load_input_A(inA, a_stream);
    load_input_B(inB, b_stream);
}

void store_output(
    hls::stream<float16>& c_stream,
    float16* outC
) {
    #pragma HLS INLINE off
    for (int i = 0; i < num_chunks_C; i++) {
        #pragma HLS PIPELINE II=1
        outC[i] = c_stream.read();
    }
}
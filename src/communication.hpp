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

void load_input_C(const float16* inC, hls::stream<float16>& c_stream) {
    #pragma HLS INLINE off
    load_C: for (int i = 0; i < num_chunks_C; i++) {
        #pragma HLS PIPELINE II=1
        c_stream.write(inC[i]);
    }
}

void load_input_D(const float16* inD, hls::stream<float16>& d_stream) {
    #pragma HLS INLINE off
    load_C: for (int i = 0; i < num_chunks_D; i++) {
        #pragma HLS PIPELINE II=1
        d_stream.write(inD[i]);
    }
}

void store_output(hls::stream<float16>& e_stream, float16* outE) {
    #pragma HLS INLINE off
    for (int i = 0; i < num_chunks_E; i++) {
        #pragma HLS PIPELINE II=1
        outE[i] = e_stream.read();
    }
}

void load_input(
    const float16* inA,
    const float16* inB,
    const float16* inC,
    const float16* inD,
    hls::stream<float16>& a_stream,
    hls::stream<float16>& b_stream,
    hls::stream<float16>& c_stream,
    hls::stream<float16>& d_stream
) {
    #pragma HLS INLINE off
    load_input_A(inA, a_stream);
    load_input_B(inB, b_stream);
    load_input_C(inC, c_stream);
    load_input_D(inD, d_stream);
}
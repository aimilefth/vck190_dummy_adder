#include "communication.hpp"

void adder(
    hls::stream<float16>& a_stream,
    hls::stream<float16>& b_stream,
    hls::stream<float16>& c_stream, // NEW Input
    hls::stream<float16>& d_stream  // Output
){
    #pragma HLS INLINE off
    for(int i=0; i<num_chunks_A; i++){
        #pragma HLS PIPELINE II=1
        float16 a_vec = a_stream.read();
        float16 b_vec = b_stream.read();
        float16 c_vec = c_stream.read(); // NEW
        float16 d_vec;
        for(int w=0; w<16; w++){
            #pragma HLS UNROLL
            d_vec[w] = a_vec[w] + b_vec[w] + c_vec[w];
        }
        d_stream.write(d_vec);
    }
}

void gemm(float16 inA[num_chunks_A], float16 inB[num_chunks_B], float16 inC[num_chunks_C], float16 outD[num_chunks_D]) {
    // AXI Ports. Map inC to gmem2 to maximize bandwidth if available, otherwise shares bundle
    #pragma HLS INTERFACE mode=m_axi port=inA  bundle=gmem0 offset=slave max_widen_bitwidth=512 num_read_outstanding=1 max_read_burst_length=64
    #pragma HLS INTERFACE mode=m_axi port=inB  bundle=gmem0 offset=slave max_widen_bitwidth=512 num_read_outstanding=1 max_read_burst_length=64
    #pragma HLS INTERFACE mode=m_axi port=inC  bundle=gmem0 offset=slave max_widen_bitwidth=512 num_read_outstanding=1 max_read_burst_length=64
    #pragma HLS INTERFACE mode=m_axi port=outD bundle=gmem0 offset=slave max_widen_bitwidth=512 num_write_outstanding=1 max_write_burst_length=64

    #pragma HLS INTERFACE mode=s_axilite port=inA  bundle=control
    #pragma HLS INTERFACE mode=s_axilite port=inB  bundle=control
    #pragma HLS INTERFACE mode=s_axilite port=inC  bundle=control
    #pragma HLS INTERFACE mode=s_axilite port=outD bundle=control
    #pragma HLS INTERFACE mode=s_axilite port=return bundle=control
    #ifndef __SYNTHESIS__
    assert(!((TILE_N > 1) && (TILE_M == 1)) && "TILE_N>1 AND TILE_M==1 Do not work");
    assert(!((GEMM_N/TILE_N) < 16) && "TILE_N makes Column width of Tile (GEMM_N/TILE_N) < 16. This is not working.");
    #endif
    // Streams between DDR and on-chip buffers
    // This could go to #Tiles - 1 -> num_chunks_A/TILE_M * (TILE_M-1)
    hls::stream<float16, num_chunks_A> input_a_stream;
    hls::stream<float16, num_chunks_B> input_b_stream;
    hls::stream<float16, num_chunks_C> input_c_stream; // NEW
    hls::stream<float16, num_chunks_D> output_d_stream;

    #pragma HLS bind_storage variable=input_a_stream type=FIFO impl=URAM
    #pragma HLS bind_storage variable=input_b_stream type=FIFO impl=URAM
    #pragma HLS bind_storage variable=input_c_stream type=FIFO impl=URAM
    #pragma HLS bind_storage variable=output_d_stream type=FIFO impl=URAM
    
    load_input(inA, inB, inC, input_a_stream, input_b_stream, input_c_stream);
    adder(input_a_stream, input_b_stream, input_c_stream, output_d_stream);
    store_output(output_d_stream, outD);
}
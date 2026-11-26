`timescale 1ns / 1ps

// Bench only: a programmable delay for pins, standing in for the Tiny
// Tapeout pad and multiplexer path (about 10 ns each way, 20 ns round trip
// measured on TT3.5 silicon).
//
// d is sampled by a fast clock (200 MHz, 5 ns) and comes out sel stages
// later: the delay is between 5*sel and 5*(sel+1) ns. sel = 0 bypasses the
// line. Every bit of d goes through the same stages, so signals that the
// sender orders by a full clock (a byte before its strobe, the output
// enable before the handoff toggle) keep that order.
module dly_line #(
    parameter int W = 1
) (
    input  logic         clk,
    input  logic [3:0]   sel,
    input  logic [W-1:0] d,
    output logic [W-1:0] q
);
    logic [15:0][W-1:0] sr_q;
    logic [W-1:0]       out_q;

    always_ff @(posedge clk) begin
        sr_q  <= {sr_q[14:0], d};
        out_q <= sr_q[sel - 4'd1];
    end

    always_comb begin
        q = (sel == 4'd0) ? d : out_q;
    end
endmodule

// A delay for pins, in the place of the pads and the multiplexer between
// the pins of a Tiny Tapeout chip and the design in it.
//
// While `enable` is high, `d` goes through three registers of a 200 MHz
// clock and comes out 10 to 15 ns later; Tiny Tapeout names up to about
// 10 ns each way (asc2-kb/tt-io.md). Every bit goes through the same
// registers, so signals that the sender orders by a clock of the chip keep
// their order. While `enable` is low, `q` is `d`.
module pin_delay #(
    parameter int W = 1
) (
    input  logic         clk,
    input  logic         enable,
    input  logic [W-1:0] d,
    output logic [W-1:0] q
);
    logic [2:0][W-1:0] line_q;

    always_ff @(posedge clk) begin
        line_q <= {line_q[1:0], d};
    end

    always_comb begin
        q = enable ? line_q[2] : d;
    end
endmodule

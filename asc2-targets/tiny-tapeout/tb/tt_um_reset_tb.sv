// The reset of the chip in a simulator with unknown values.
//
// Verilator, which runs the other tests, gives every flop a 0 or a 1 at
// the start. A flop of the real chip starts with either, and a simulator
// such as Icarus Verilog marks it as unknown until something writes it. A
// chip that is built well leaves the unknown values by its reset alone.
//
// The test holds the reset, checks that every output pin has its level of
// the reset after 12 clocks, releases the reset, and waits for the first
// request of the CPU: the fetch of the first instruction, with the header
// 0x2c on the bus.
//
// The test is plain enough for Icarus Verilog. It runs on the Verilog that
// sv2v makes of the sources and on the gates of the layout run.
`timescale 1ns/1ps

module tt_um_reset_tb;
    localparam int RESET_KNOWN_CLOCKS = 12;
    localparam int RESET_CLOCKS       = 40;
    localparam int FETCH_CLOCKS       = 100;

    // The fetch of an instruction with its address, in kernel mode
    localparam logic [7:0] HDR_FIRST_FETCH = 8'h2c;

    // Bit 4 is the silent serial line. Every other output is low.
    localparam logic [7:0] UO_AT_RESET = 8'h10;

    logic       clk = 1'b0;
    logic       rst_n = 1'b0;
    logic [7:0] uo_out;
    logic [7:0] uio_out;
    logic [7:0] uio_oe;

    int clocks = 0;

    tt_um_toksaitov_almurut2 chip (
        .ui_in   (8'h00),
        .uo_out  (uo_out),
        .uio_in  (8'h00),
        .uio_out (uio_out),
        .uio_oe  (uio_oe),
        .ena     (1'b1),
        .clk     (clk),
        .rst_n   (rst_n)
    );

    always #40 clk = ~clk;

    always @(posedge clk) begin
        clocks = clocks + 1;

        if (clocks >= RESET_KNOWN_CLOCKS && clocks <= RESET_CLOCKS) begin
            if (uo_out !== UO_AT_RESET || uio_oe !== 8'h00) begin
                $display("FAILED: tt_um_reset_tb, clock %0d of the reset: uo_out is %b and uio_oe is %b", clocks, uo_out, uio_oe);
                $fatal(1);
            end
        end

        if (clocks == RESET_CLOCKS) begin
            rst_n = 1'b1;
        end

        // FRAME is high and the chip drives the bus: the first byte of a request
        if (clocks > RESET_CLOCKS && uo_out[3] === 1'b1 && uio_oe === 8'hff) begin
            if (uio_out !== HDR_FIRST_FETCH) begin
                $display("FAILED: tt_um_reset_tb, the first byte on the bus is %h", uio_out);
                $fatal(1);
            end
            $display("passed: tt_um_reset_tb");
            $finish;
        end

        if (clocks == RESET_CLOCKS + FETCH_CLOCKS) begin
            $display("FAILED: tt_um_reset_tb, no request %0d clocks after the reset: uo_out is %b and uio_oe is %b", FETCH_CLOCKS, uo_out, uio_oe);
            $fatal(1);
        end
    end
endmodule

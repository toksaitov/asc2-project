// The gates of a layout as the test program asc2-tests/chip/asc_trace.cpp
// sees the chip: the Tiny Tapeout top module, as asc_trace_top.sv there
// gives it. The gates do not show the registers of the CPU, so the program
// can only run them on their own time (-t) and compare what comes out of
// the pins: the requests on the bus and the console.
module asc_trace_top (
    input  logic         clk,
    input  logic         rst_n,
    input  logic [7:0]   ui_in,
    output logic [7:0]   uo_out,
    input  logic [7:0]   uio_in,
    output logic [7:0]   uio_out,
    output logic [7:0]   uio_oe,
    // The gates have no timer that the test steps
    input  logic         test_tick_on,
    input  logic         test_tick,
    // What the hardware description shows of the CPU; the gates show none
    // of it
    output logic         cpu_boundary,
    output logic         cpu_fetch,
    output logic         cpu_retired,
    output logic         cpu_halted,
    output logic         cpu_double_fault,
    output logic [31:0]  cpu_ip,
    output logic [31:0]  cpu_inst,
    output logic [383:0] cpu_regs,
    output logic [5:0]   cpu_flags,
    output logic [5:0]   cpu_cr0,
    output logic [31:0]  cpu_cr1,
    output logic [31:0]  cpu_cr2,
    output logic [31:0]  cpu_cr3,
    output logic [5:0]   cpu_cr4,
    output logic [31:0]  cpu_cr5,
    output logic         cpu_dbg_locked
);
    tt_um_toksaitov_almurut2 chip (
        .ui_in        (ui_in),
        .uo_out       (uo_out),
        .uio_in       (uio_in),
        .uio_out      (uio_out),
        .uio_oe       (uio_oe),
        .ena          (1'b1),
        .clk          (clk),
        .rst_n        (rst_n)
    );

    // The test program runs the chip on its own time and takes the end of
    // an instruction only as a beat for the console and for the events, so
    // a beat once in 64 clocks stands in for it. A run ends with the clocks
    // that the command line allows.
    logic [5:0] beat_q;

    always_ff @(posedge clk) begin
        beat_q <= beat_q + 6'd1;
    end

    always_comb begin
        cpu_boundary     = 1'b1;
        cpu_fetch        = 1'b0;
        cpu_retired      = (beat_q == 6'd0);
        cpu_halted       = 1'b0;
        cpu_double_fault = 1'b0;
        cpu_ip           = 32'd0;
        cpu_inst         = 32'd0;
        cpu_regs         = 384'd0;
        cpu_flags        = 6'd0;
        cpu_cr0          = 6'd0;
        cpu_cr1          = 32'd0;
        cpu_cr2          = 32'd0;
        cpu_cr3          = 32'd0;
        cpu_cr4          = 6'd0;
        cpu_cr5          = 32'd0;
        cpu_dbg_locked   = 1'b0;
    end
endmodule

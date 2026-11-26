`timescale 1ns / 1ps

// Lint-only stand-in for the Quartus altpll primitive, with only the
// ports and parameters bench_mem uses. Quartus uses its own altpll; this
// file is never part of the Quartus project.
/* verilator lint_off UNUSEDSIGNAL */
/* verilator lint_off UNUSEDPARAM */
module altpll #(
    parameter string intended_device_family = "MAX 10",
    parameter string operation_mode         = "NORMAL",
    parameter string pll_type               = "AUTO",
    parameter int    inclk0_input_frequency = 20000,
    parameter int    clk0_multiply_by       = 1,
    parameter int    clk0_divide_by         = 1,
    parameter int    clk0_duty_cycle        = 50,
    parameter string clk0_phase_shift       = "0",
    parameter int    clk1_multiply_by       = 1,
    parameter int    clk1_divide_by         = 1,
    parameter int    clk1_duty_cycle        = 50,
    parameter string clk1_phase_shift       = "0",
    parameter string compensate_clock       = "CLK0",
    parameter int    width_clock            = 5,
    parameter string lpm_type               = "altpll"
) (
    input  logic [1:0]             inclk,
    output logic [width_clock-1:0] clk,
    output logic                   locked
);
    always_comb begin
        clk    = {width_clock{inclk[0]}};
        locked = 1'b1;
    end
endmodule
/* verilator lint_on UNUSEDPARAM */
/* verilator lint_on UNUSEDSIGNAL */

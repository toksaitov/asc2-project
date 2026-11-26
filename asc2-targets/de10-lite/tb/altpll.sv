// A model of Quartus's PLL for lint and the test: output 0 has four times
// the frequency of the 50 MHz input, which is all that `asc2.sv` uses.
/* verilator lint_off UNUSEDPARAM */
/* verilator lint_off UNUSEDSIGNAL */
module altpll #(
    parameter intended_device_family = "",
    parameter operation_mode         = "",
    parameter pll_type               = "",
    parameter inclk0_input_frequency = 0,
    parameter clk0_multiply_by       = 0,
    parameter clk0_divide_by         = 0,
    parameter clk0_duty_cycle        = 0,
    parameter clk0_phase_shift       = "",
    parameter compensate_clock       = "",
    parameter width_clock            = 0,
    parameter lpm_type               = ""
) (
    input  logic [1:0]             inclk,
    output logic [width_clock-1:0] clk
);
    localparam realtime HALF = 2.5ns;

    logic out;

    // Two of the four pulses at each edge of the input
    initial begin
        out = 1'b0;
        forever begin
            @(inclk[0]);
            out = 1'b1;
            #HALF out = 1'b0;
            #HALF out = 1'b1;
            #HALF out = 1'b0;
        end
    end

    assign clk = {{(width_clock - 1){1'b0}}, out};
endmodule

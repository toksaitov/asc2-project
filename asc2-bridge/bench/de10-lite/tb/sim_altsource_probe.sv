`timescale 1ns / 1ps

// Simulation model of altsource_probe for tb_bench_mem: the test bench
// sets src_v through a hierarchical reference and reads probe the same
// way. Never part of the Quartus project.
/* verilator lint_off UNUSEDSIGNAL */
/* verilator lint_off UNUSEDPARAM */
module altsource_probe #(
    parameter string sld_auto_instance_index = "YES",
    parameter int    sld_instance_index      = 0,
    parameter string instance_id             = "NONE",
    parameter int    probe_width             = 1,
    parameter int    source_width            = 1,
    parameter string source_initial_value    = "0",
    parameter string enable_metastability    = "NO",
    parameter string lpm_type                = "altsource_probe"
) (
    input  logic [probe_width - 1:0]  probe,
    output logic [source_width - 1:0] source,
    input  logic                      source_clk,
    input  logic                      source_ena
);
    logic [source_width - 1:0] src_v = '0;

    always_comb begin
        source = src_v;
    end
endmodule
/* verilator lint_on UNUSEDPARAM */
/* verilator lint_on UNUSEDSIGNAL */

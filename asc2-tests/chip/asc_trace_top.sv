// The chip as the test program asc_trace.cpp sees it: the Tiny Tapeout
// top module, and next to its pins the registers of the CPU, which the
// pins do not show. The test compares them with the registers of the
// emulator after every instruction.
//
// The module is for simulation only: it reads the registers through
// their paths inside the chip. The chip is built with ASC_TEST_TICK
// here, so that the test can step the timer once per instruction.
// With test_tick_on low the timer steps once in 256 clocks, as in the
// chip that is built.
module asc_trace_top (
    input  logic         clk,
    input  logic         rst_n,
    input  logic [7:0]   ui_in,
    output logic [7:0]   uo_out,
    input  logic [7:0]   uio_in,
    output logic [7:0]   uio_out,
    output logic [7:0]   uio_oe,
    // The test steps the timer, and one step of the timer
    input  logic         test_tick_on,
    input  logic         test_tick,
    // The CPU is between two instructions, starts a fetch, has finished
    // an instruction
    output logic         cpu_boundary,
    output logic         cpu_fetch,
    output logic         cpu_retired,
    output logic         cpu_halted,
    output logic         cpu_double_fault,
    output logic [31:0]  cpu_ip,
    output logic [31:0]  cpu_inst,
    // r0 to r7, fp, the stack pointer of kernel mode, lr, and the stack
    // pointer of user mode, 32 bits each, r0 in the lowest bits
    output logic [383:0] cpu_regs,
    output logic [5:0]   cpu_flags,
    output logic [5:0]   cpu_cr0,
    output logic [31:0]  cpu_cr1,
    output logic [31:0]  cpu_cr2,
    output logic [31:0]  cpu_cr3,
    output logic [5:0]   cpu_cr4,
    output logic [31:0]  cpu_cr5,
    // The kernel has locked the debug port
    output logic         cpu_dbg_locked
);
    tt_um_toksaitov_almurut2 chip (
        .ui_in        (ui_in),
        .uo_out       (uo_out),
        .uio_in       (uio_in),
        .uio_out      (uio_out),
        .uio_oe       (uio_oe),
        .test_tick_on (test_tick_on),
        .test_tick    (test_tick),
        .ena          (1'b1),
        .clk          (clk),
        .rst_n        (rst_n)
    );

    always_comb begin
        cpu_boundary     = (chip.soc.cpu.state_q == chip.soc.cpu.S_BOUNDARY);
        cpu_fetch        = (chip.soc.cpu.state_q == chip.soc.cpu.S_FETCH);
        cpu_retired      = (chip.soc.cpu.state_q == chip.soc.cpu.S_RETIRE);
        cpu_halted       = chip.soc.cpu.halted_q;
        cpu_double_fault = chip.soc.cpu.double_fault_q;
        cpu_ip           = chip.soc.cpu.ip_q;
        cpu_inst         = chip.soc.cpu.ir_q;
        for (int i = 0; i < 12; i++) begin
            cpu_regs[32 * i +: 32] = chip.soc.cpu.regs.words_q[i];
        end
        cpu_flags        = chip.soc.cpu.flags_q;
        cpu_cr0          = chip.soc.cpu.cr0_q;
        cpu_cr1          = chip.soc.cpu.cr1_q;
        cpu_cr2          = chip.soc.cpu.cr2_q;
        cpu_cr3          = {chip.soc.cpu.cr3_q, 12'd0};
        cpu_cr4          = chip.soc.cpu.cr4_q;
        cpu_cr5          = {5'd0, chip.soc.cpu.cr5_q};
        cpu_dbg_locked   = chip.soc.cpu.dbg_lock_q;
    end
endmodule

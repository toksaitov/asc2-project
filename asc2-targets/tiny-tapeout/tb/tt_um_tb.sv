// Test of the Tiny Tapeout top module: the chip as it is built, seen from
// its pins.
//
// A small model of the RP2040 answers the bus as asc2-kb/bus-protocol-v1.md
// describes it: the chip changes REQ for every byte it sends and for every
// byte it has taken, and the model answers each change with a change of
// ACK. The test serves a program of six instructions, one transaction after
// another, and so says for each what the chip must ask for. It checks the
// pins and not the inside of the chip:
//
//   1. During the reset the chip drives no bus pin, and the free outputs
//      have their fixed levels.
//   2. While STOP is high the CPU starts nothing, and the debug port shows
//      a CPU that waits at its reset address.
//   3. With STOP low the CPU runs on its own: it fetches from the boot ROM,
//      writes a word to RAM, lets interrupts in, and halts.
//   4. A short pulse on the interrupt line is noise. A level wakes the CPU:
//      it reads the address of the handler from the vector table and
//      fetches there.
//   5. The debug port shows ip, the instruction, the cause of the trap, the
//      flags, and cr0, and after them the registers of the program and the
//      record of the trap in cr1 and cr4. Its last three bits are zeros:
//      the port is open, and the program has left the two upper bits of
//      cr0 alone.
//
// asc2-tests/chip/ runs whole programs on the same top module and compares
// every instruction with the emulator; it also drives the two pins of the
// keyboard. asc2-tests/bus/ tests the bus unit with every disturbance of
// the wires.
module tt_um_tb;
    localparam realtime     CLK_HALF   = 10ns;    // a 50 MHz clock
    localparam realtime     RP_DELAY   = 43ns;    // the model answers out of step with the clock
    localparam int unsigned WAIT_LIMIT = 100000;  // clocks to wait for the chip before giving up
    localparam int unsigned END_CLOCKS = 40;      // clocks for the chip to end a transaction and run an instruction
    localparam int unsigned DBG_BITS   = 643;

    // Headers of requests without the two low bits, which the bus unit sets
    localparam logic [7:0]  HDR_FETCH    = 8'h2c;  // a fetch of 4 bytes in kernel mode with its address
    localparam logic [7:0]  HDR_WRITE    = 8'ha4;  // a write of 4 bytes in kernel mode
    localparam logic [7:0]  HDR_OWN_READ = 8'h24;  // a read of 4 bytes by the CPU itself

    // The program, at the start of the boot ROM
    localparam logic [31:0] INST_LI_R1   = 32'h00fb_0190;  // li   r1, 0xfb   the mask that lets line 2 in
    localparam logic [31:0] INST_LI_R2   = 32'h0021_0290;  // li   r2, 0x21   the data port of the interrupt controller
    localparam logic [31:0] INST_OUTB    = 32'h0000_1207;  // outb r1, r2
    localparam logic [31:0] INST_SR      = 32'h0010_1014;  // sr   r1, r0, 16
    localparam logic [31:0] INST_STI     = 32'h0000_0003;  // sti
    localparam logic [31:0] INST_HLT     = 32'h0000_0000;  // hlt

    localparam logic [31:0] RESET_IP     = 32'h8080_0000;
    localparam logic [31:0] ROM_START    = 32'h0080_0000;
    localparam logic [31:0] RAM_WORD     = 32'h0000_0010;    // the word that sr writes
    localparam logic [31:0] VECTOR_WORD  = 32'h0000_0028;    // vector 10: line 2 of the interrupt controller
    localparam logic [31:0] HANDLER      = 32'h8080_0040;

    // Values of the debug port: flags Z, I; cr0 F, T
    localparam logic [5:0]  FLAGS_RESET   = 6'h02;
    localparam logic [5:0]  FLAGS_HALTED  = 6'h12;
    localparam logic [3:0]  CR0_RESET     = 4'h4;
    localparam logic [3:0]  CR0_IN_TRAP   = 4'hc;
    localparam logic [26:0] CAUSE_LINE_2  = 27'h000_000a;
    localparam logic [4:0]  CPU_AT_REST   = 5'd0;  // the state between two instructions
    localparam logic [2:0]  BUS_DONE      = 3'd0;  // the status of a transaction that has gone well

    logic       clk;
    logic       rst_n;
    logic [7:0] ui_in;
    logic [7:0] uo_out;
    logic [7:0] uio_in;
    logic [7:0] uio_out;
    logic [7:0] uio_oe;

    // The pins by their names
    logic       req;
    logic       dbg_do;
    logic       chip_drives;
    logic       frame;
    logic       ack;
    logic       irq;
    logic       dbg_ck;
    logic       dbg_di;
    logic       kbd_clk;
    logic       kbd_data;
    logic       stop;

    // The RP2040's side of the bus pins. The board pulls a free pin up.
    logic       rp_drives;
    logic [7:0] rp_out;
    logic       seq;       // the sequence bit that the next header must carry

    int unsigned errors;

    tt_um_toksaitov_almurut2 chip (
        .ui_in   (ui_in),
        .uo_out  (uo_out),
        .uio_in  (uio_in),
        .uio_out (uio_out),
        .uio_oe  (uio_oe),
        .ena     (1'b1),
        .clk     (clk),
        .rst_n   (rst_n)
    );

    always_comb begin
        req         = uo_out[0];
        dbg_do      = uo_out[1];
        chip_drives = uo_out[2];
        frame       = uo_out[3];
        ui_in       = {stop, kbd_data, kbd_clk, dbg_di, 1'b0, dbg_ck, irq, ack};
        uio_in      = rp_drives ? rp_out : 8'hff;
    end

    initial begin
        clk = 1'b0;
        forever begin
            #CLK_HALF clk = ~clk;
        end
    end

    task automatic check(input logic ok, input string what);
        if (!ok) begin
            $display("FAILED at %0t: %s", $realtime, what);
            ++errors;
        end
    endtask

    task automatic wait_clocks(input int unsigned count);
        repeat (count) begin
            @(posedge clk);
        end
    endtask

    // Watches the bus for `count` clocks, in which the chip must start nothing.
    task automatic check_quiet(input int unsigned count, input string what);
        logic old_req = req;
        logic quiet   = 1'b1;

        repeat (count) begin
            @(posedge clk);
            if (req != old_req || frame || chip_drives || uio_oe != 8'h00) begin
                quiet = 1'b0;
            end
        end
        check(quiet, what);
    endtask

    /* The model of the RP2040 */

    // Waits for the next change of REQ.
    task automatic wait_req();
        logic        old_req = req;
        int unsigned waited  = 0;

        while (req == old_req && waited < WAIT_LIMIT) begin
            @(posedge clk);
            ++waited;
        end
        check(req != old_req, "the chip did not change REQ");
        check(!(rp_drives && uio_oe != 8'h00), "the chip and the RP2040 drive the bus together");
    endtask

    task automatic answer();
        #RP_DELAY ack = ~ack;
    endtask

    // Takes a byte that the chip sends.
    task automatic take(output logic [7:0] value);
        wait_req();
        check(uio_oe == 8'hff && chip_drives, "the chip does not drive the byte it sends");
        value = uio_out;
        answer();
    endtask

    // Gives the chip a byte and waits until the chip has taken it.
    task automatic give(input logic [7:0] value);
        rp_out    = value;
        rp_drives = 1'b1;
        answer();
        wait_req();
    endtask

    // Serves one transaction: takes the header, the address, and the bytes
    // of a write, and gives the status and the bytes of a read. The chip
    // must ask for what is expected; `expected_wdata` is zero for a read,
    // and a write does not use `rdata`.
    task automatic serve(
        input logic [7:0]  expected_hdr,
        input logic [31:0] expected_addr,
        input logic [31:0] expected_wdata,
        input logic [31:0] rdata,
        input string       what
    );
        logic [7:0]  hdr;
        logic [31:0] addr;
        logic [31:0] wdata;
        int unsigned size;
        logic [7:0]  value;

        addr  = '0;
        wdata = '0;
        take(hdr);
        check(frame, "FRAME is low inside a transaction");
        size = 1 << hdr[5:4];

        // A request for "the next word" carries no address
        if (!hdr[6]) begin
            take(addr[7:0]);
            take(addr[15:8]);
            take(addr[23:16]);
            take(addr[31:24]);
        end
        if (hdr[7]) begin
            for (int unsigned i = 0; i < size; ++i) begin
                take(value);
                wdata[8 * i +: 8] = value;
            end
        end

        // The chip hands the bus over: it has stopped driving before this change of REQ
        wait_req();
        check(uio_oe == 8'h00 && !chip_drives, "the chip drives the bus after it has handed it over");

        // The status repeats the sequence bit of the header; the code 0 is "done"
        give({hdr[0], ~hdr[0], 6'b000000});
        if (!hdr[7]) begin
            for (int unsigned i = 0; i < size; ++i) begin
                give(rdata[8 * i +: 8]);
            end
        end

        // The RP2040 hands the bus back
        rp_drives = 1'b0;
        answer();

        check({hdr[7:2], 2'b00} == expected_hdr && addr == expected_addr && wdata == expected_wdata, what);

        // The sequence bit changes with every transaction that has gone well
        check(hdr[1:0] == {1'b0, seq}, "the two low bits of the header are not a zero and the sequence bit");
        seq = ~seq;
    endtask

    task automatic serve_read(
        input logic [7:0]  expected_hdr,
        input logic [31:0] expected_addr,
        input logic [31:0] rdata,
        input string       what
    );
        serve(expected_hdr, expected_addr, 32'd0, rdata, what);
    endtask

    task automatic serve_write(
        input logic [7:0]  expected_hdr,
        input logic [31:0] expected_addr,
        input logic [31:0] expected_wdata,
        input string       what
    );
        serve(expected_hdr, expected_addr, expected_wdata, 32'd0, what);
    endtask

    /* The debug port */

    // Reads the bits of the debug port. A rise of its clock selects bit 0
    // if its data input is high, and the next bit otherwise.
    task automatic read_debug(output logic [DBG_BITS-1:0] word);
        for (int unsigned i = 0; i < DBG_BITS; ++i) begin
            dbg_di = (i == 0);
            wait_clocks(4);
            dbg_ck = 1'b1;
            wait_clocks(4);
            dbg_ck = 1'b0;
            wait_clocks(4);
            word[i] = dbg_do;
        end
    endtask

    /* The test */

    initial begin
        /* verilator lint_off UNUSEDSIGNAL */
        logic [DBG_BITS-1:0] shown;  // bits 126:111 are the bus unit's own state, which this test leaves alone
        /* verilator lint_on UNUSEDSIGNAL */
        logic                old_req;

        errors    = 0;
        rst_n     = 1'b0;
        ack       = 1'b0;
        irq       = 1'b0;
        dbg_ck    = 1'b0;
        dbg_di    = 1'b0;
        kbd_clk   = 1'b1;
        kbd_data  = 1'b1;
        stop      = 1'b1;
        rp_drives = 1'b0;
        rp_out    = 8'h00;
        seq       = 1'b0;

        // 1. The reset
        wait_clocks(40);
        check(uio_oe == 8'h00 && !chip_drives, "the chip drives the bus during the reset");
        check(!req && !frame, "REQ or FRAME is high during the reset");
        check(uo_out[7:4] == 4'b0001, "the free outputs do not have their levels");
        rst_n = 1'b1;

        // 2. STOP holds the CPU before its first instruction
        check_quiet(300, "the CPU has started a transaction while STOP is high");
        read_debug(shown);
        check(shown[31:0] == RESET_IP, "the debug port does not show the reset address in ip");
        check(shown[96:91] == FLAGS_RESET && shown[100:97] == CR0_RESET, "the debug port does not show flags and cr0 after a reset");
        check(shown[127] && shown[105:101] == CPU_AT_REST && !shown[106] && !shown[107], "the debug port does not show a CPU that waits between two instructions");
        check(shown[642:640] == 3'b000, "the debug port does not show an open port and zeros in the upper bits of cr0 after a reset");

        // 3. The program
        stop = 1'b0;
        serve_read(HDR_FETCH, ROM_START, INST_LI_R1, "the first request is not a fetch at the start of the boot ROM");
        serve_read(HDR_FETCH, ROM_START + 32'd4, INST_LI_R2, "the second request is not a fetch of the second instruction");
        serve_read(HDR_FETCH, ROM_START + 32'd8, INST_OUTB, "the third request is not a fetch of the third instruction");
        serve_read(HDR_FETCH, ROM_START + 32'd12, INST_SR, "the fourth request is not a fetch of the fourth instruction");
        serve_write(HDR_WRITE, RAM_WORD, 32'h0000_00fb, "sr does not write r1 to its word of RAM");
        serve_read(HDR_FETCH, ROM_START + 32'd16, INST_STI, "the request after the write is not a fetch of the fifth instruction");
        serve_read(HDR_FETCH, ROM_START + 32'd20, INST_HLT, "the next request is not a fetch of the sixth instruction");
        wait_clocks(END_CLOCKS);
        check_quiet(300, "the halted CPU has started a transaction");
        check(uio_oe == 8'h00 && !chip_drives && !frame, "the bus is not released after the transactions");

        read_debug(shown);
        check(shown[31:0] == RESET_IP + 32'd20 && shown[63:32] == INST_HLT, "the debug port does not show the hlt and its address");
        check(shown[96:91] == FLAGS_HALTED && shown[100:97] == CR0_RESET, "the debug port does not show flags with I set");
        check(shown[127] && shown[105:101] == CPU_AT_REST && shown[106] && !shown[107], "the debug port does not show a halted CPU");
        check(shown[110:108] == BUS_DONE, "the debug port does not show the status of the last transaction");
        check(shown[159:128] == 32'd0 && shown[191:160] == 32'h0000_00fb && shown[223:192] == 32'h0000_0021, "the debug port does not show r0, r1, and r2");
        check(shown[511:224] == 288'd0, "the debug port does not show zeros in the registers that the program has left alone");

        // 4. The interrupt line. A pulse of two clocks is noise and starts nothing.
        old_req = req;
        irq = 1'b1;
        wait_clocks(2);
        irq = 1'b0;
        wait_clocks(200);
        check(req == old_req && !frame, "a short pulse on the interrupt line has started a transaction");

        irq = 1'b1;
        serve_read(HDR_OWN_READ, VECTOR_WORD, HANDLER, "the CPU does not read vector 10 for the interrupt line");
        serve_read(HDR_FETCH, {1'b0, HANDLER[30:0]}, INST_HLT, "the CPU does not fetch at the address of the handler");
        wait_clocks(END_CLOCKS);
        check_quiet(300, "the CPU has started a transaction in the handler with interrupts off");
        irq = 1'b0;

        // 5. The record of the trap on the debug port
        read_debug(shown);
        check(shown[31:0] == HANDLER, "the debug port does not show the address of the handler in ip");
        check(shown[90:64] == CAUSE_LINE_2, "the debug port does not show vector 10 in cr5");
        check(shown[96:91] == FLAGS_RESET && shown[100:97] == CR0_IN_TRAP, "the debug port does not show flags and cr0 inside a handler");
        check(shown[127] && shown[105:101] == CPU_AT_REST && shown[106] && !shown[107], "the debug port does not show a halted CPU in the handler");
        check(shown[543:512] == RESET_IP + 32'd24, "the debug port does not show the address after the hlt in cr1");
        check(shown[639:608] == {26'd0, FLAGS_HALTED}, "the debug port does not show the flags before the interrupt in cr4");
        check(shown[191:160] == 32'h0000_00fb, "the debug port does not show r1 inside the handler");

        if (errors == 0) begin
            $display("passed: tt_um_tb");
            $finish;
        end else begin
            $display("FAILED: tt_um_tb, %0d checks", errors);
            $fatal(1);
        end
    end
endmodule

// The start of the chip on the Tiny Tapeout demo board, as far as a
// simulation can copy it. asc2-kb/tt-devboard.md describes the board, and
// asc2-kb/bus-protocol-v1.md gives the rules that the firmware of its RP2350
// follows ("Rules for the devkit").
//
// The test copies four things that the other tests leave out:
//
//   * The multiplexer of Tiny Tapeout. A design that is not selected gets
//     no clock, and its reset and its inputs are low. The flops of the chip
//     hold whatever the power has left in them: the test is run with
//     +verilator+rand+reset+2, which gives every flop a value by chance.
//   * The reset pin of the two revisions of the board. On v3.2 a capacitor
//     makes the level rise and fall slowly, and the chip may see many edges
//     where the RP2350 or the reset button has made one. On v3.3 an inverter
//     gives one clean edge, at any moment of the clock.
//   * The reset button of the board, which a person presses at any moment:
//     inside a transaction, and while the RP2350 drives the bus.
//   * The pins of the bus while nobody drives them. The plusarg +pull says
//     what such a pin reads: 0 a low level, 1 a high level, 2 a level that
//     changes by chance.
//
// The steps:
//
//   1. The design is selected as the stock firmware of the board selects
//      one: with the reset high and the clock running. The chip has never
//      had a reset, so it may do anything, and the RP2350 drives nothing.
//   2. The start by the rules: the reset low, 20 clocks, the clock stopped,
//      the reset released, 200 us, the clock started. After the 11th clock
//      of the reset the pins must have their levels of the reset, whatever
//      the chip did before.
//   3. STOP holds the CPU; the debug port is open and shows the reset
//      address. Then the program runs.
//   4. The reset button, pressed inside a request, with the clock running
//      and ACK high. The chip must start anew with the first fetch.
//   5. The reset button, pressed while the RP2350 gives the bytes of an
//      answer. The same.
//   6. The program runs to its end.
//
// At no moment may the chip and the RP2350 drive the bus together. The test
// looks at the pins only, so it runs on the gates of a layout run as well.
//
// The plusarg +board is 2 for the revision v3.2 and 3 for v3.3.
module tt_um_powerup_tb;
    localparam realtime     CLK_HALF     = 10ns;    // a 50 MHz clock
    localparam realtime     RP_DELAY     = 43ns;    // the model answers out of step with the clock
    localparam int unsigned WAIT_LIMIT   = 100000;  // clocks to wait for the chip before giving up
    localparam int unsigned END_CLOCKS   = 40;      // clocks for the chip to end a transaction and run an instruction
    localparam int unsigned DBG_BITS     = 643;

    // The rules of the firmware (bus-protocol-v1.md)
    localparam int unsigned RESET_CLOCKS = 20;      // clocks with the reset low
    localparam int unsigned KNOWN_CLOCKS = 11;      // the chip has its values of the reset after this many of them
    localparam realtime     SETTLE_TIME  = 200us;   // from the release of the reset to the start of the clock

    // The reset pin of v3.2: how long the level stays between low and high
    localparam realtime     RISE_BY_RP     = 40us;   // the RP2350 charges the capacitor
    localparam realtime     FALL_BY_BUTTON = 100us;  // the button empties it through 1 kOhm
    localparam realtime     RISE_BY_BOARD  = 300us;  // the pull-up of the board charges it
    localparam realtime     BUTTON_HELD    = 300us;

    // Headers of requests without the two low bits, which the bus unit sets
    localparam logic [7:0]  HDR_FETCH    = 8'h2c;  // a fetch of 4 bytes in kernel mode with its address
    localparam logic [7:0]  HDR_WRITE    = 8'ha4;  // a write of 4 bytes in kernel mode

    // The program, at the start of the boot ROM
    localparam logic [31:0] INST_LI_R1   = 32'h00fb_0190;  // li   r1, 0xfb
    localparam logic [31:0] INST_SR      = 32'h0010_1014;  // sr   r1, r0, 16
    localparam logic [31:0] INST_HLT     = 32'h0000_0000;  // hlt

    localparam logic [31:0] RESET_IP     = 32'h8080_0000;
    localparam logic [31:0] ROM_START    = 32'h0080_0000;
    localparam logic [31:0] RAM_WORD     = 32'h0000_0010;  // the word that sr writes

    // Values of the debug port: flags Z; cr0 F
    localparam logic [5:0]  FLAGS_RESET  = 6'h02;
    localparam logic [3:0]  CR0_RESET    = 4'h4;
    localparam logic [4:0]  CPU_AT_REST  = 5'd0;  // the state between two instructions

    // Bit 4 is the silent serial line. Every other output is low after a reset.
    localparam logic [7:0]  UO_AT_RESET  = 8'h10;

    int unsigned board;
    int unsigned pull;

    // The pins of the chip, behind the multiplexer of Tiny Tapeout
    logic       clk;
    logic       rst_n;
    logic [7:0] ui_in;
    logic [7:0] uo_out;
    logic [7:0] uio_in;
    logic [7:0] uio_out;
    logic [7:0] uio_oe;

    // The board
    logic       selected;   // the multiplexer has selected the design
    logic       clk_on;     // the RP2350 makes the clock
    logic       pad_clk;
    logic       pad_rst_n;
    logic [7:0] pad_uio;
    logic [7:0] free_pins;  // what the pins of the bus read while nobody drives them

    // The outputs of the chip by their names
    logic       req;
    logic       dbg_do;
    logic       chip_drives;
    logic       frame;

    // The RP2350's side
    logic       ack;
    logic       dbg_ck;
    logic       dbg_di;
    logic       stop;
    logic       rp_drives;
    logic [7:0] rp_out;
    logic       seq;        // the sequence bit that the next header must carry
    logic       seen_req;   // the level of REQ that the model has answered last
    logic       watch;      // the test checks that the two sides never drive together

    int unsigned errors;

    tt_um_toksaitov_almurut2 chip (
        .ui_in   (ui_in),
        .uo_out  (uo_out),
        .uio_in  (uio_in),
        .uio_out (uio_out),
        .uio_oe  (uio_oe),
        .ena     (selected),
        .clk     (clk),
        .rst_n   (rst_n)
    );

    always_comb begin
        req         = uo_out[0];
        dbg_do      = uo_out[1];
        chip_drives = uo_out[2];
        frame       = uo_out[3];

        // A pin of the bus has the level of the side that drives it
        for (int i = 0; i < 8; ++i) begin
            if (selected && uio_oe[i]) begin
                pad_uio[i] = uio_out[i];
            end else if (rp_drives) begin
                pad_uio[i] = rp_out[i];
            end else begin
                pad_uio[i] = free_pins[i];
            end
        end

        // A design that is not selected gets low levels and no clock. The
        // two lines of the keyboard are high: nothing is connected to them.
        clk    = selected && pad_clk;
        rst_n  = selected && pad_rst_n;
        ui_in  = selected ? {stop, 2'b11, dbg_di, 1'b0, dbg_ck, 1'b0, ack} : 8'h00;
        uio_in = selected ? pad_uio : 8'h00;
    end

    initial begin
        pad_clk = 1'b0;
        forever begin
            #CLK_HALF;
            if (clk_on) begin
                pad_clk = ~pad_clk;
            end
        end
    end

    // Free pins that change by chance do so out of step with the clock
    initial begin
        free_pins = 8'h00;
        forever begin
            #7ns;
            case (pull)
                0:       free_pins = 8'h00;
                1:       free_pins = 8'hff;
                default: free_pins = 8'($urandom);
            endcase
        end
    end

    task automatic check(input logic ok, input string what);
        if (!ok) begin
            $display("FAILED at %0t: %s", $realtime, what);
            ++errors;
        end
    endtask

    // Two outputs against each other on one pin
    always @(uio_oe or rp_drives or selected) begin
        if (watch && selected && rp_drives && uio_oe != 8'h00) begin
            check(1'b0, "the chip and the RP2350 drive the bus together");
        end
    end

    task automatic wait_clocks(input int unsigned count);
        repeat (count) begin
            @(posedge pad_clk);
        end
    endtask

    task automatic start_clock();
        clk_on = 1'b1;
    endtask

    // Stops the clock at its low level
    task automatic stop_clock();
        @(negedge pad_clk);
        clk_on = 1'b0;
    endtask

    task automatic check_quiet(input int unsigned count, input string what);
        logic old_req = req;
        logic quiet   = 1'b1;

        repeat (count) begin
            @(posedge pad_clk);
            if (req != old_req || frame || chip_drives || uio_oe != 8'h00) begin
                quiet = 1'b0;
            end
        end
        check(quiet, what);
    endtask

    // The pins of a chip that is held in reset, for `count` clocks
    task automatic check_reset_levels(input int unsigned count, input string what);
        logic good = 1'b1;

        repeat (count) begin
            @(posedge pad_clk);
            if (uo_out != UO_AT_RESET || uio_oe != 8'h00) begin
                good = 1'b0;
            end
        end
        check(good, what);
    endtask

    /* The reset pin */

    // The level of the reset pin goes to `level`. On v3.2 it takes `slow`
    // to get there, and until then the chip may see either level; on v3.3
    // it gets there at once, at any moment of the clock.
    //
    // The delays are numbers of nanoseconds by chance, never zero.
    /* verilator lint_off ZERODLY */
    task automatic move_reset(input logic level, input realtime slow);
        realtime left;
        realtime step;

        if (board == 2) begin
            left = slow;
            while (left > 0) begin
                step = $urandom_range(1, 400) * 1ns;
                #step pad_rst_n = 1'($urandom);
                left = left - step;
            end
        end else begin
            step = $urandom_range(1, 40) * 1ns;
            #step;
        end
        pad_rst_n = level;
    endtask
    /* verilator lint_on ZERODLY */

    // What the firmware does when it sees the reset pin low, whoever has
    // pulled it: it lets go of the bus and forgets the transaction. REQ is
    // low after a reset. ACK stays as it is: the chip takes its level of
    // the reset as the one to start from.
    task automatic forget_transaction();
        rp_drives = 1'b0;
        seq       = 1'b0;
        seen_req  = 1'b0;
    endtask

    // A person presses the reset button while the clock runs
    task automatic press_button(input string what);
        // The firmware sees the first low level of the pin
        @(negedge pad_clk);
        pad_rst_n = 1'b0;
        #RP_DELAY forget_transaction();
        move_reset(1'b0, FALL_BY_BUTTON);

        #BUTTON_HELD;
        check_reset_levels(40, what);

        move_reset(1'b1, RISE_BY_BOARD);

        // The firmware waits until the level has been high for a while and
        // listens to no pin of the chip before
        #SETTLE_TIME;
    endtask

    /* The model of the RP2350 */

    // Waits for the next change of REQ. After a reset the chip may have
    // changed it already, before the model has begun to listen.
    task automatic wait_req();
        int unsigned waited = 0;

        while (req == seen_req && waited < WAIT_LIMIT) begin
            @(posedge pad_clk);
            ++waited;
        end
        check(req != seen_req, "the chip did not change REQ");
        seen_req = req;
    endtask

    task automatic answer();
        #RP_DELAY ack = ~ack;
    endtask

    // Takes a byte that the chip sends.
    task automatic take(output logic [7:0] value);
        wait_req();
        check(uio_oe == 8'hff && chip_drives, "the chip does not drive the byte it sends");
        value = pad_uio;
        answer();
    endtask

    // Gives the chip a byte and waits until the chip has taken it.
    task automatic give(input logic [7:0] value);
        rp_out    = value;
        rp_drives = 1'b1;
        answer();
        wait_req();
    endtask

    // Takes the header and the address of a request, and the bytes of a
    // write, and waits until the chip has handed the bus over.
    task automatic take_request(
        input logic [7:0]  expected_hdr,
        input logic [31:0] expected_addr,
        input logic [31:0] expected_wdata,
        input string       what
    );
        logic [7:0]  hdr;
        logic [31:0] addr;
        logic [31:0] wdata;
        logic [7:0]  value;

        addr  = '0;
        wdata = '0;
        take(hdr);
        check(frame, "FRAME is low inside a transaction");
        take(addr[7:0]);
        take(addr[15:8]);
        take(addr[23:16]);
        take(addr[31:24]);
        if (hdr[7]) begin
            for (int unsigned i = 0; i < 4; ++i) begin
                take(value);
                wdata[8 * i +: 8] = value;
            end
        end

        // The chip hands the bus over: it has stopped driving before this change of REQ
        wait_req();
        check(uio_oe == 8'h00 && !chip_drives, "the chip drives the bus after it has handed it over");

        check({hdr[7:2], 2'b00} == expected_hdr && addr == expected_addr && wdata == expected_wdata, what);
        check(hdr[1:0] == {1'b0, seq}, "the two low bits of the header are not a zero and the sequence bit");
    endtask

    // Gives the status: the sequence bit of the header twice, the second
    // time inverted, and the code 0, "done".
    task automatic give_status();
        give({seq, ~seq, 6'b000000});
    endtask

    // Hands the bus back; the sequence bit changes with every transaction that has gone well
    task automatic hand_back();
        rp_drives = 1'b0;
        answer();
        seq = ~seq;
    endtask

    task automatic serve_fetch(input logic [31:0] expected_addr, input logic [31:0] inst, input string what);
        take_request(HDR_FETCH, expected_addr, 32'd0, what);
        give_status();
        for (int unsigned i = 0; i < 4; ++i) begin
            give(inst[8 * i +: 8]);
        end
        hand_back();
    endtask

    task automatic serve_write(input logic [31:0] expected_addr, input logic [31:0] expected_wdata, input string what);
        take_request(HDR_WRITE, expected_addr, expected_wdata, what);
        give_status();
        hand_back();
    endtask

    /* The debug port */

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
        logic [7:0]          value;  // a byte of a request that the reset button cuts off
        /* verilator lint_on UNUSEDSIGNAL */
        int unsigned         clocks;

        if (!$value$plusargs("board=%d", board)) begin
            board = 3;
        end
        if (!$value$plusargs("pull=%d", pull)) begin
            pull = 1;
        end

        errors    = 0;
        selected  = 1'b0;
        clk_on    = 1'b0;
        pad_rst_n = 1'b1;  // the board pulls the pin high
        ack       = 1'b0;
        dbg_ck    = 1'b0;
        dbg_di    = 1'b0;
        stop      = 1'b0;
        rp_drives = 1'b0;
        rp_out    = 8'h00;
        seq       = 1'b0;
        seen_req  = 1'b0;
        watch     = 1'b1;

        // The power is on, and no design is selected
        #5us;

        // 1. The stock firmware selects the design: the clock runs, and the
        //    chip has had no reset. Nothing is expected of it.
        selected = 1'b1;
        start_clock();
        wait_clocks(3000);

        // 2. The start by the rules. The firmware holds the reset low for
        //    20 clocks and listens to no pin; from the 12th clock on the
        //    pins must have their levels.
        move_reset(1'b0, FALL_BY_BUTTON);
        forget_transaction();
        stop = 1'b1;
        wait_clocks(KNOWN_CLOCKS);
        check_reset_levels(RESET_CLOCKS - KNOWN_CLOCKS, "the pins do not have their levels after 11 clocks of the first reset");
        stop_clock();
        move_reset(1'b1, RISE_BY_RP);
        #SETTLE_TIME;
        start_clock();

        // 3. STOP holds the CPU before its first instruction, and the debug port is open
        check_quiet(300, "the CPU has started a transaction while STOP is high");
        read_debug(shown);
        check(shown[31:0] == RESET_IP, "the debug port does not show the reset address in ip");
        check(shown[96:91] == FLAGS_RESET && shown[100:97] == CR0_RESET, "the debug port does not show flags and cr0 after a reset");
        check(shown[127] && shown[105:101] == CPU_AT_REST && !shown[106] && !shown[107], "the debug port does not show a CPU that waits between two instructions");
        check(shown[642:640] == 3'b000, "the debug port does not show an open port and zeros in the upper bits of cr0 after a reset");

        stop = 1'b0;
        serve_fetch(ROM_START, INST_LI_R1, "the first request is not a fetch at the start of the boot ROM");

        // 4. The reset button inside a request: the chip has sent the header
        //    and a byte of the address. ACK is high now, and the chip must
        //    start from that level after the reset.
        take(value);
        take(value);
        check(ack, "ACK is not high at the reset button; the test has lost its case");
        press_button("the pins do not have their levels while the reset button is held inside a request");
        serve_fetch(ROM_START, INST_LI_R1, "the first request after the reset button is not a fetch at the start of the boot ROM");
        serve_fetch(ROM_START + 32'd4, INST_SR, "the second request is not a fetch of the second instruction");
        serve_write(RAM_WORD, 32'h0000_00fb, "sr does not write r1 to its word of RAM");

        // 5. The reset button while the RP2350 gives the bytes of an answer
        take_request(HDR_FETCH, ROM_START + 32'd8, 32'd0, "the request after the write is not a fetch of the third instruction");
        give_status();
        give(INST_HLT[7:0]);
        press_button("the pins do not have their levels while the reset button is held inside an answer");

        // The chip drives the bus again some clocks after the reset: the
        // firmware has that long to let go of the bus
        clocks = 0;
        move_reset(1'b0, FALL_BY_BUTTON);
        forget_transaction();
        wait_clocks(RESET_CLOCKS);
        @(negedge pad_clk);
        pad_rst_n = 1'b1;
        while (uio_oe == 8'h00 && clocks < WAIT_LIMIT) begin
            @(posedge pad_clk);
            ++clocks;
        end
        check(clocks >= 10, "the chip drives the bus less than 10 clocks after the release of the reset");
        $display("The chip drives the bus %0d clocks after the release of the reset.", clocks);

        // 6. The program, to its end
        serve_fetch(ROM_START, INST_LI_R1, "the first request after the second reset button is not a fetch at the start of the boot ROM");
        serve_fetch(ROM_START + 32'd4, INST_SR, "the second request after the second reset button is not a fetch of the second instruction");
        serve_write(RAM_WORD, 32'h0000_00fb, "sr does not write r1 to its word of RAM after the second reset button");
        serve_fetch(ROM_START + 32'd8, INST_HLT, "the last request is not a fetch of the third instruction");
        wait_clocks(END_CLOCKS);
        check_quiet(300, "the halted CPU has started a transaction");
        check(uio_oe == 8'h00 && !chip_drives && !frame, "the bus is not released after the transactions");

        read_debug(shown);
        check(shown[31:0] == RESET_IP + 32'd8 && shown[63:32] == INST_HLT, "the debug port does not show the hlt and its address");
        check(shown[191:160] == 32'h0000_00fb, "the debug port does not show r1");
        check(shown[127] && shown[105:101] == CPU_AT_REST && shown[106] && !shown[107], "the debug port does not show a halted CPU");

        if (errors == 0) begin
            $display("passed: tt_um_powerup_tb (board v3.%0d, +pull=%0d)", board, pull);
            $finish;
        end else begin
            $display("FAILED: tt_um_powerup_tb, %0d checks (board v3.%0d, pull %0d)", errors, board, pull);
            $fatal(1);
        end
    end
endmodule

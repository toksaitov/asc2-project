// Test of the board's top module: the chip in the DE10-Lite, seen from the
// wires of the Pico.
//
// asc2-targets/tiny-tapeout/tb/tt_um_tb.sv tests the pins of the chip. This
// test checks what the board adds around them: the eight wires of the bus
// that the two sides share, the guard that keeps the FPGA's drivers off
// while the Pico drives, the slide switch of the clock, the slide switch of
// the delays, the two sources of STOP, the wires of the keyboard, and the
// LEDs.
//
// A small model of the Pico answers the bus as asc2-kb/bus-protocol-v1.md
// describes it and serves a program of three instructions. The wires of
// the bus have pull-ups here, so wires that nobody drives read as ones.
//
//   1. During the reset nobody drives the bus, and LEDR9 is on.
//   2. While SW9 is up the CPU starts nothing, and neither does it while
//      the Pico holds pi_stop high.
//   3. With both low the CPU fetches from the boot ROM over the shared
//      wires, writes a word to RAM, and halts. The model raises pi_stop
//      when the header of a fetch has come, as the Pico does for a step:
//      the CPU must run that one instruction and wait.
//   4. While the chip sends a byte, a high level on pi_drives takes the
//      byte off the wires, and a low level puts it back.
//   5. The LEDs follow their wires.
//   6. The two wires of the keyboard reach their two pins of the chip.
//   7. The three wires of the debug port reach the chip: the model reads
//      the 128 bits as the Pico does. After the step of the first
//      instruction it finds that instruction and the address of the
//      second, and at the end the hlt, its address, and the halted CPU.
//
// The seven steps run twice: with SW8 up, when the clock of the chip is the
// Pico's and the clock of the board stands still, and with SW8 down, when
// it is the other way round. Then they run twice more with SW7 up: the
// delays need the clock of the board, which runs in both. In all four runs
// every change of the watched pins must come at its time: at once with SW7
// down, and 10 to 15 ns later with SW7 up.
module asc2_tb;
    localparam realtime     CLK_HALF   = 40ns;    // a 12.5 MHz clock, as on the bench
    localparam realtime     CLK50_HALF = 10ns;    // the 50 MHz clock of the board
    localparam realtime     PICO_DELAY = 43ns;    // the model answers out of step with the clock
    localparam realtime     PIN_WAIT   = 16ns;    // longer than a delay of the pins
    localparam int unsigned WAIT_LIMIT = 100000;  // clocks to wait for the chip before giving up
    localparam int unsigned END_CLOCKS = 40;      // clocks for the chip to end a transaction and run an instruction

    // The reset by rule 5 of the bus: 20 clocks at least
    localparam int unsigned RESET_CLOCKS = 24;

    // Headers of requests without the two low bits, which the bus unit sets
    localparam logic [7:0]  HDR_FETCH  = 8'h2c;  // a fetch of 4 bytes in kernel mode with its address
    localparam logic [7:0]  HDR_WRITE  = 8'ha4;  // a write of 4 bytes in kernel mode

    // The program, at the start of the boot ROM
    localparam logic [31:0] INST_LI_R1 = 32'h00fb_0190;  // li  r1, 0xfb
    localparam logic [31:0] INST_SR    = 32'h0010_1014;  // sr  r1, r0, 16
    localparam logic [31:0] INST_HLT   = 32'h0000_0000;  // hlt

    localparam logic [31:0] ROM_START  = 32'h0080_0000;
    localparam logic [31:0] RAM_WORD   = 32'h0000_0010;    // the word that sr writes

    localparam logic [7:0]  BUS_FREE   = 8'hff;          // the wires with nobody driving

    localparam int unsigned DBG_BITS   = 128;
    localparam logic [31:0] RESET_IP   = 32'h8080_0000;

    logic       tb_clk;     // the clock of this test and of its model of the Pico
    logic       clk50_on;
    logic       pi_clk_on;
    logic       clk50;
    logic       pi_clk;
    logic       pi_rst_n;
    tri1  [7:0] uio;
    logic       req;
    logic       pi_ack;
    logic       pi_irq;
    logic       pi_stop;
    logic       pi_drives;
    logic       chip_drives;
    logic       kbd_clk;
    logic       kbd_data;
    logic       pi_dbg_ck;
    logic       pi_dbg_di;
    logic       dbg_do;
    logic [9:0] SW;
    logic [9:0] LEDR;

    // The Pico's side of the bus wires
    logic       pico_oe;
    logic [7:0] pico_out;
    logic       seq;       // the sequence bit that the next header must carry

    int unsigned errors;
    int unsigned wires_wrong;  // nanoseconds in which the wires of the bus did not follow chip_drives

    asc2 board (
        .clk50       (clk50),
        .pi_rst_n    (pi_rst_n),
        .pi_clk      (pi_clk),
        .uio         (uio),
        .req         (req),
        .pi_ack      (pi_ack),
        .pi_irq      (pi_irq),
        .pi_stop     (pi_stop),
        .pi_drives   (pi_drives),
        .chip_drives (chip_drives),
        .kbd_clk     (kbd_clk),
        .kbd_data    (kbd_data),
        .pi_dbg_ck   (pi_dbg_ck),
        .pi_dbg_di   (pi_dbg_di),
        .dbg_do      (dbg_do),
        .SW          (SW),
        .LEDR        (LEDR)
    );

    assign uio = pico_oe ? pico_out : 8'bzzzz_zzzz;

    delay_watch #(.NAME ("the reset"))       watch_rst   (.is_late (SW[7]), .early (pi_rst_n), .late (board.chip.rst_n));
    delay_watch #(.NAME ("ACK"))             watch_ack   (.is_late (SW[7]), .early (pi_ack), .late (board.chip.ui_in[0]));
    delay_watch #(.NAME ("STOP"))            watch_stop  (.is_late (SW[7]), .early (board.stop), .late (board.chip.ui_in[7]));
    delay_watch #(.NAME ("a bus input"))     watch_in    (.is_late (SW[7]), .early (uio[0]), .late (board.chip.uio_in[0]));
    delay_watch #(.NAME ("REQ"))             watch_req   (.is_late (SW[7]), .early (board.chip.uo_out[0]), .late (req));
    delay_watch #(.NAME ("the debug port"))  watch_dbg   (.is_late (SW[7]), .early (board.chip.uo_out[1]), .late (dbg_do));
    delay_watch #(.NAME ("the chip drives")) watch_drive (.is_late (SW[7]), .early (board.chip.uo_out[2]), .late (chip_drives));
    delay_watch #(.NAME ("a bus output"))    watch_out   (.is_late (SW[7]), .early (board.chip.uio_out[7]), .late (board.uio_pins[7]));
    delay_watch #(.NAME ("the drivers"))     watch_oe    (.is_late (SW[7]), .early (board.chip.uio_oe[0]), .late (board.uio_oe_pins));

    initial begin
        tb_clk = 1'b0;
        forever begin
            #CLK_HALF tb_clk = ~tb_clk;
        end
    end

    // The two clocks of the board. Each runs only while the test lets it.
    assign pi_clk = tb_clk && pi_clk_on;

    initial begin
        clk50 = 1'b0;
        forever begin
            #CLK50_HALF clk50 = clk50_on && !clk50;
        end
    end

    // The chip's byte is on the wires as long as chip_drives says so and the
    // Pico does not drive, and nothing is on them while nobody drives.
    initial begin
        wires_wrong = 0;
        forever begin
            #1ns;
            if (!pico_oe && !pi_drives && uio != (chip_drives ? board.uio_pins : BUS_FREE)) begin
                ++wires_wrong;
            end
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
            @(posedge tb_clk);
        end
    endtask

    // Watches the bus for `count` clocks, in which the chip must start nothing.
    task automatic check_quiet(input int unsigned count, input string what);
        logic old_req = req;
        logic quiet   = 1'b1;

        repeat (count) begin
            @(posedge tb_clk);
            if (req != old_req || chip_drives || uio != BUS_FREE) begin
                quiet = 1'b0;
            end
        end
        check(quiet, what);
    endtask

    // The LEDs, from LEDR0 up: REQ, ACK, the chip drives, the Pico drives,
    // FRAME, IRQ, STOP, the clock is the Pico's, the delays are on, and the
    // reset. FRAME has no wire to the Pico, so the test names its level.
    task automatic check_leds(input logic frame, input string what);
        check(LEDR == {!pi_rst_n, SW[7], SW[8], SW[9] || pi_stop, pi_irq, frame, pi_drives, chip_drives, pi_ack, req}, what);
    endtask

    /* The model of the Pico */

    // Waits for the next change of REQ.
    task automatic wait_req();
        logic        old_req = req;
        int unsigned waited  = 0;

        while (req == old_req && waited < WAIT_LIMIT) begin
            @(posedge tb_clk);
            ++waited;
        end
        check(req != old_req, "the chip did not change REQ");
        check(!(pico_oe && chip_drives), "the chip and the Pico drive the bus together");
    endtask

    task automatic answer();
        #PICO_DELAY pi_ack = ~pi_ack;
    endtask

    // Takes a byte that the chip sends.
    task automatic take(output logic [7:0] value);
        wait_req();
        check(chip_drives, "the chip does not say that it drives the byte it sends");
        value = uio;
        answer();
    endtask

    // Gives the chip a byte and waits until the chip has taken it. The Pico
    // says that it drives before it does.
    task automatic give(input logic [7:0] value);
        check(!chip_drives, "the chip still drives when the Pico starts to");
        pi_drives = 1'b1;
        pico_out  = value;
        pico_oe   = 1'b1;
        answer();
        wait_req();
    endtask

    // Serves one transaction: takes the header, the address, and the bytes
    // of a write, and gives the status and the bytes of a read. With
    // `try_guard` set it tries the guard on the header. With `stop_after`
    // set it raises pi_stop when the header has come.
    task automatic serve(
        input logic [7:0]  expected_hdr,
        input logic [31:0] expected_addr,
        input logic [31:0] expected_wdata,
        input logic [31:0] rdata,
        input logic        try_guard,
        input logic        stop_after,
        input string       what
    );
        logic [7:0]  hdr;
        logic [31:0] addr;
        logic [31:0] wdata;
        int unsigned size;
        logic [7:0]  value;

        addr  = '0;
        wdata = '0;

        wait_req();
        check(chip_drives, "the chip does not say that it drives the header");
        hdr = uio;
        check_leds(1'b1, "the LEDs do not show the wires inside a transaction");
        if (try_guard) begin
            // 4. The guard. The Pico says that it drives and does not.
            pi_drives = 1'b1;
            #1ns;
            check(uio == BUS_FREE, "the FPGA drives the bus while the Pico says that it drives");
            check_leds(1'b1, "the LEDs do not show that the Pico drives");
            pi_drives = 1'b0;
            #1ns;
            check(uio == hdr, "the header is not back on the bus after the Pico has let go");
        end
        if (stop_after) begin
            pi_stop = 1'b1;
        end
        answer();
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
        check(!chip_drives && uio == BUS_FREE, "the chip drives the bus after it has handed it over");

        // The status repeats the sequence bit of the header; the code 0 is "done"
        give({hdr[0], ~hdr[0], 6'b000000});
        if (!hdr[7]) begin
            for (int unsigned i = 0; i < size; ++i) begin
                give(rdata[8 * i +: 8]);
            end
        end

        // The Pico hands the bus back
        pico_oe   = 1'b0;
        pi_drives = 1'b0;
        answer();

        check({hdr[7:2], 2'b00} == expected_hdr && addr == expected_addr && wdata == expected_wdata, what);

        // The sequence bit changes with every transaction that has gone well
        check(hdr[1:0] == {1'b0, seq}, "the two low bits of the header are not a zero and the sequence bit");
        seq = ~seq;
    endtask

    /* The debug port */

    // Reads the bits of the debug port as the Pico does. A rise of its
    // clock selects bit 0 if its data input is high, and the next bit
    // otherwise.
    task automatic read_debug(output logic [DBG_BITS-1:0] word);
        for (int unsigned i = 0; i < DBG_BITS; ++i) begin
            pi_dbg_di = (i == 0);
            wait_clocks(4);
            pi_dbg_ck = 1'b1;
            wait_clocks(4);
            pi_dbg_ck = 1'b0;
            wait_clocks(4);
            word[i] = dbg_do;
        end
    endtask

    /* The test */

    // The seven steps with one of the two clocks. The other one stands still, but the delays need the clock of the board.
    task automatic run(input logic is_pi_clk, input logic is_late);
        /* verilator lint_off UNUSEDSIGNAL */
        logic [DBG_BITS-1:0] shown;  // this test looks at ip, the instruction, and three flags
        /* verilator lint_on UNUSEDSIGNAL */

        pi_rst_n  = 1'b0;
        pi_ack    = 1'b0;
        pi_irq    = 1'b0;
        pi_stop   = 1'b0;
        pi_drives = 1'b0;
        kbd_clk   = 1'b1;
        kbd_data  = 1'b1;
        pi_dbg_ck = 1'b0;
        pi_dbg_di = 1'b0;
        SW[9:8]   = {1'b1, is_pi_clk};
        pico_oe   = 1'b0;
        pico_out  = 8'h00;
        seq       = 1'b0;
        pi_clk_on = is_pi_clk;
        clk50_on  = !is_pi_clk || is_late;

        // 1. The reset. SW7 moves inside it, when the clocks run.
        wait_clocks(RESET_CLOCKS);
        SW[7] = is_late;
        wait_clocks(RESET_CLOCKS);
        check(!chip_drives && uio == BUS_FREE, "the chip drives the bus during the reset");
        check(!req, "REQ is high during the reset");
        check_leds(1'b0, "the LEDs do not show the reset");
        pi_rst_n = 1'b1;

        // 2. SW9 holds the CPU before its first instruction
        check_quiet(300, "the CPU has started a transaction while SW9 is up");
        check_leds(1'b0, "the LEDs do not show a CPU that is held");

        // The Pico holds the CPU in the same way
        pi_stop = 1'b1;
        #1ns;
        SW[9]   = 1'b0;
        check_quiet(300, "the CPU has started a transaction while pi_stop is high");
        check_leds(1'b0, "the LEDs do not show a CPU that the Pico holds");

        // 3. The program, the first two instructions one at a time
        pi_stop = 1'b0;
        serve(HDR_FETCH, ROM_START, 32'd0, INST_LI_R1, 1'b1, 1'b1, "the first request is not a fetch at the start of the boot ROM");
        check_quiet(300, "the CPU has gone on to its second instruction while pi_stop is high");

        // After a step the debug port shows the instruction that has run and the address of the next one
        read_debug(shown);
        check(shown[31:0] == RESET_IP + 32'd4 && shown[63:32] == INST_LI_R1 && shown[127],
              "the debug port does not show the first instruction and the address of the second after a step");
        pi_stop = 1'b0;
        serve(HDR_FETCH, ROM_START + 32'd4, 32'd0, INST_SR, 1'b0, 1'b1, "the second request is not a fetch of the second instruction");
        serve(HDR_WRITE, RAM_WORD, 32'h0000_00fb, 32'd0, 1'b0, 1'b0, "sr does not write r1 to its word of RAM while pi_stop is high");
        check_quiet(300, "the CPU has gone on to its third instruction while pi_stop is high");
        pi_stop = 1'b0;
        serve(HDR_FETCH, ROM_START + 32'd8, 32'd0, INST_HLT, 1'b0, 1'b0, "the request after the write is not a fetch of the third instruction");
        wait_clocks(END_CLOCKS);
        check_quiet(300, "the halted CPU has started a transaction");

        // 5. The LEDs at rest, and the wire of the interrupt line
        check_leds(1'b0, "the LEDs do not show the wires at rest");
        pi_irq = 1'b1;
        #1ns;
        check_leds(1'b0, "LEDR5 does not show the interrupt line");
        pi_irq = 1'b0;

        // 6. The wires of the keyboard, each one low alone
        check(board.ui_in[6:5] == 2'b11, "the pins of the keyboard are not high at rest");
        kbd_data = 1'b0;
        #PIN_WAIT;
        check(board.ui_in[6:5] == 2'b01, "the data line of the keyboard does not reach its pin of the chip");
        kbd_data = 1'b1;
        kbd_clk  = 1'b0;
        #PIN_WAIT;
        check(board.ui_in[6:5] == 2'b10, "the clock line of the keyboard does not reach its pin of the chip");
        kbd_clk  = 1'b1;

        // 7. The wires of the debug port
        read_debug(shown);
        check(shown[31:0] == RESET_IP + 32'd8 && shown[63:32] == INST_HLT, "the debug port does not show the hlt and its address");
        check(shown[127] && shown[106] && !shown[107], "the debug port does not show a halted CPU");
    endtask

    initial begin
        errors = 0;
        SW     = '0;
        run(1'b1, 1'b0);
        run(1'b0, 1'b0);
        run(1'b1, 1'b1);
        run(1'b0, 1'b1);

        // Every watched signal has changed, and none of its changes came at another time
        check(watch_rst.changes > 0 && watch_rst.errors == 0, "the reset does not come at its time");
        check(watch_ack.changes > 0 && watch_ack.errors == 0, "ACK does not come at its time");
        check(watch_stop.changes > 0 && watch_stop.errors == 0, "STOP does not come at its time");
        check(watch_in.changes > 0 && watch_in.errors == 0, "a bus input does not come at its time");
        check(watch_req.changes > 0 && watch_req.errors == 0, "REQ does not come at its time");
        check(watch_dbg.changes > 0 && watch_dbg.errors == 0, "the debug port does not come at its time");
        check(watch_drive.changes > 0 && watch_drive.errors == 0, "the line of the chip's driving does not come at its time");
        check(watch_out.changes > 0 && watch_out.errors == 0, "a bus output does not come at its time");
        check(watch_oe.changes > 0 && watch_oe.errors == 0, "the chip's drivers do not come at their time");
        check(wires_wrong == 0, "the wires of the bus do not follow chip_drives");

        if (errors == 0) begin
            $display("passed: asc2_tb");
            $finish;
        end else begin
            $display("FAILED: asc2_tb, %0d checks", errors);
            $fatal(1);
        end
    end
endmodule

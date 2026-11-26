`timescale 1ns / 1ps

// Simulation of bench_mem (the whole Test 4 DE10 design) against a
// behavioral RP with the bench/firmware memory map: RAM with a kernel page, a
// flash window holding h(offset, fseed), device registers, a disk of 64
// sectors that takes time to write, a console input stream that the chip
// must echo, and interrupts at random times.
//
// The RP checks that the two sides never drive uio together and counts
// what the chip did; the design's own probes must show zero errors and
// every kind of transaction.
//
// Bus protocol v1: the RP echoes the sequence bit in its status byte,
// answers a repeat from its copy of the last answer, and drops back to
// waiting for a header when REQ stays still for RP_TO mid-transaction.
// The link tests inject ACK glitches, three kinds of RP faults, a
// scrambled bus state and torn edges into IDLE (the design's own tear
// test, in each of its four states); every one must be recovered with no
// data error, no LINK_ERROR and no contention.
module tb_bench_mem;
    localparam int unsigned RAM_SIZE    = 32'h30000;
    localparam int unsigned KERNEL_BASE = 32'h2F000;
    localparam int unsigned FLASH_BASE  = 32'h800000;
    localparam int unsigned FLASH_WIN   = 32'hC0000;
    localparam int unsigned DEV_BASE    = 32'hFF0000;
    localparam int unsigned DEV_ID      = 32'h324D4C41;
    localparam int unsigned FSEED       = 32'h5EED0F1A;
    localparam int unsigned SEED        = 32'h0BADF00D;

    logic        clk50 = 1'b0;
    logic        pi_clk = 1'b0;
    logic        pi_rst_n = 1'b0;
    logic        pi_ack = 1'b0;
    logic        ack_gl = 1'b0;   // XORed onto ACK: glitch injection
    logic        pi_irq = 1'b0;
    logic        pdrv = 1'b0;
    logic        req;
    logic        dbg_do;
    logic [7:0]  rp_out = '0;
    logic        rp_oe = 1'b0;
    wire  [7:0]  uio;
    logic [9:0]  ledr;
    logic [7:0]  hex0, hex1, hex2, hex3, hex4, hex5;
    real         clk_half = 10.0;

    assign uio = rp_oe ? rp_out : 8'bzzzz_zzzz;

    always begin
        #10 clk50 = ~clk50;
    end

    always begin
        #(clk_half) pi_clk = ~pi_clk;
    end

    // Short chip timeout (1023 clocks: 20 us at 50 MHz) so faults resolve
    // quickly; the RP model's own timeout is RP_TO.
    bench_mem #(
        .BUS_TO_BITS (10)
    ) dut (
        .clk50     (clk50),
        .pi_rst_n  (pi_rst_n),
        .pi_clk    (pi_clk),
        .uio       (uio),
        .req       (req),
        .pi_ack    (pi_ack ^ ack_gl),
        .pi_irq    (pi_irq),
        .pi_dbg_ck (1'b0),
        .pi_dbg_di (pdrv),
        .dbg_do    (dbg_do),
        .LEDR      (ledr),
        .HEX0      (hex0),
        .HEX1      (hex1),
        .HEX2      (hex2),
        .HEX3      (hex3),
        .HEX4      (hex4),
        .HEX5      (hex5)
    );

    always begin
        #1;
        if (rp_oe && dbg_do) begin
            $fatal(1, "contention: chip and RP both drive uio at %0t", $time);
        end
    end

    /* Host side: sources and probes */

    logic [199:0] src = '0;

    task automatic put(input int lsb, input int width, input logic [63:0] v);
        for (int i = 0; i < width; i++) begin
            src[lsb + i] = v[i];
        end
        dut.u_issp_a.src_v = src;
    endtask

    function automatic logic [63:0] get(input int lsb, input int width);
        logic [63:0] v;
        v = '0;
        for (int i = 0; i < width; i++) begin
            v[i] = dut.probe_a[lsb + i];
        end
        return v;
    endfunction

    // Link counters (T4C): 0 timeouts, 1 bad answers, 2 bad states,
    // 3 transactions ended with LINK_ERROR.
    function automatic int unsigned link(input int k);
        return dut.probe_c[224 + 32 * k +: 32];
    endfunction

    // Tears applied by the design's tear test (T4C).
    function automatic int unsigned tears();
        return dut.probe_c[479:448];
    endfunction

    function automatic logic [31:0] count(input int k);
        if (k == 14) begin
            return {16'h0, dut.probe_b[495:480]};  // overlap events
        end
        if (k == 15) begin
            return dut.probe_b[479:448];           // long overlap events
        end
        return dut.probe_b[32 * k +: 32];
    endfunction

    /* RP model */

    function automatic int unsigned h(input int unsigned a, input int unsigned s);
        int unsigned x;
        x = (a & 32'hFFFFFF) ^ s;
        x = x ^ (x << 13);
        x = x ^ (x >> 17);
        x = x ^ (x << 5);
        return x;
    endfunction

    function automatic int unsigned con_byte(input int unsigned j);
        return h(j, 32'hC0DE0000) & 32'hFF;
    endfunction

    byte unsigned ram [RAM_SIZE];
    int unsigned  disk [64][1024];
    int unsigned  scratch = 0;
    int unsigned  rp_tx = 0;
    int unsigned  rp_faults = 0;
    bit           nv [2] = '{0, 0};
    int unsigned  na [2] = '{0, 0};
    int unsigned  disk_sec = 0;
    int unsigned  disk_buf = 0;
    int unsigned  disk_cmd = 0;
    int unsigned  disk_done = 0;
    bit           disk_busy = 0;
    bit           disk_err = 0;
    int unsigned  con_deliv = 0;
    int unsigned  con_echo = 0;
    int unsigned  con_bad = 0;
    int unsigned  con_t0 = 0;
    bit           irq_on = 0;
    longint       irq_t = 0;
    int unsigned  irq_count = 0;
    longint       irq_lat_max = 0;

    function automatic int unsigned now_us();
        return int'($time / 1000);
    endfunction

    // The disk works in its own process: the register write only stages
    // the command (and, for a write, the data, as bench/firmware does).
    int unsigned  disk_staged [1024];
    int unsigned  disk_go_cmd = 0;
    int unsigned  disk_go_sec = 0;
    int unsigned  disk_go_buf = 0;

    task automatic disk_start(input int unsigned cmd);
        if (disk_busy || disk_sec >= 64 || (disk_buf & 3) != 0 || disk_buf + 4096 > RAM_SIZE
            || (cmd != 1 && cmd != 2)) begin
            disk_err = 1;
            return;
        end
        disk_busy   = 1;
        disk_go_sec = disk_sec;
        disk_go_buf = disk_buf;
        if (cmd == 2) begin
            for (int i = 0; i < 1024; i++) begin
                disk_staged[i] = {ram[disk_buf + 4 * i + 3], ram[disk_buf + 4 * i + 2],
                                  ram[disk_buf + 4 * i + 1], ram[disk_buf + 4 * i]};
            end
        end
        disk_go_cmd = cmd;
    endtask

    initial begin : disk_engine
        forever begin
            wait (disk_go_cmd != 0);
            if (disk_go_cmd == 2) begin
                #(50000 + $urandom % 150000);
                for (int i = 0; i < 1024; i++) begin
                    disk[disk_go_sec][i] = disk_staged[i];
                end
            end else begin
                #(10000 + $urandom % 20000);
                for (int i = 0; i < 1024; i++) begin
                    {ram[disk_go_buf + 4 * i + 3], ram[disk_go_buf + 4 * i + 2],
                     ram[disk_go_buf + 4 * i + 1], ram[disk_go_buf + 4 * i]} = disk[disk_go_sec][i];
                end
            end
            disk_go_cmd = 0;
            disk_done++;
            disk_busy = 0;
        end
    end

    task automatic access(input logic [7:0] hdr, input int unsigned a_in, input int unsigned w,
                          output logic [7:0] st, output int unsigned rd);
        bit          wr;
        bit          nx;
        int unsigned size;
        int unsigned kind;
        bit          kernel;
        int unsigned a;
        wr     = hdr[7];
        nx     = hdr[6];
        size   = (hdr[5:4] == 0) ? 1 : (hdr[5:4] == 1) ? 2 : (hdr[5:4] == 2) ? 4 : 0;
        kind   = hdr[3];
        kernel = hdr[2];
        a      = a_in;
        st     = 8'h00;
        rd     = 0;
        if (nx) begin
            if (!nv[kind]) begin
                st = 8'h04;
            end else begin
                a = na[kind];
            end
        end
        if (st == 0 && size == 0) begin
            st = 8'h01;
        end
        if (st == 0 && (a % size) != 0) begin
            st = 8'h03;
        end
        if (st == 0) begin
            if (a + size <= RAM_SIZE) begin
                if (!kernel && a + size > KERNEL_BASE) begin
                    st = 8'h02;
                end else if (wr) begin
                    for (int i = 0; i < size; i++) begin
                        ram[a + i] = w[8 * i +: 8];
                    end
                end else begin
                    for (int i = 0; i < size; i++) begin
                        rd[8 * i +: 8] = ram[a + i];
                    end
                end
            end else if (a >= FLASH_BASE && a + size <= FLASH_BASE + FLASH_WIN) begin
                if (wr) begin
                    st = 8'h01;
                end else begin
                    int unsigned wv;
                    wv = h((a - FLASH_BASE) & ~32'h3, FSEED);
                    rd = wv >> (8 * (a & 3));
                    if (size < 4) begin
                        rd = rd & ((32'h1 << (8 * size)) - 1);
                    end
                end
            end else if (a >= DEV_BASE && a < DEV_BASE + 32'h30) begin
                if (size != 4) begin
                    st = 8'h01;
                end else if (kind == 1) begin
                    st = 8'h02;
                end else begin
                    case (a - DEV_BASE)
                        32'h00: if (wr) st = 8'h01; else rd = DEV_ID;
                        32'h04: if (wr) scratch = w; else rd = scratch;
                        32'h08: if (wr) st = 8'h01; else rd = rp_tx;
                        32'h0C: if (wr) st = 8'h01; else rd = now_us();
                        32'h10: begin
                            if (wr) begin
                                st = 8'h01;
                            end else begin
                                rd = {30'b0, 1'b1, con_deliv < (now_us() - con_t0) / 8};
                            end
                        end
                        32'h14: begin
                            if (wr) begin
                                if (w[7:0] != con_byte(con_echo)) begin
                                    con_bad++;
                                end
                                con_echo++;
                            end else if (con_deliv < (now_us() - con_t0) / 8) begin
                                rd = con_byte(con_deliv);
                                con_deliv++;
                            end else begin
                                rd = 32'h100;
                            end
                        end
                        32'h18: begin
                            if (wr) begin
                                if (pi_irq) begin
                                    if ($time - irq_t > irq_lat_max) begin
                                        irq_lat_max = $time - irq_t;
                                    end
                                    irq_count++;
                                    pi_irq = 1'b0;
                                end
                            end else begin
                                rd = {31'b0, pi_irq};
                            end
                        end
                        32'h20: if (wr) disk_sec = w; else rd = disk_sec;
                        32'h24: if (wr) disk_buf = w; else rd = disk_buf;
                        32'h28: begin
                            if (wr) begin
                                disk_cmd = w;
                                disk_start(w);
                            end else begin
                                rd = disk_cmd;
                            end
                        end
                        32'h2C: if (wr) st = 8'h01; else rd = {disk_done[15:0], 14'b0, disk_err, disk_busy};
                        default: st = 8'h01;
                    endcase
                end
            end else begin
                st = 8'h01;
            end
        end
        if (st == 0) begin
            na[kind] = a + size;
            nv[kind] = 1;
        end else begin
            nv[kind] = 0;
            rp_faults++;
        end
        rp_tx++;
    endtask

    logic req_level = 1'b0;

    task automatic wait_req();
        wait (req !== req_level && pi_rst_n);
        req_level = req;
        #(($urandom % 60) + 1);
    endtask

    // v1: mid-transaction waits give up after RP_TO ns.
    localparam int RP_TO = 4000;

    task automatic wait_req_to(output bit ok);
        int t;
        ok = 1'b0;
        t  = 0;
        while (t < RP_TO) begin
            if (req !== req_level && pi_rst_n) begin
                ok = 1'b1;
                break;
            end
            #1;
            t++;
        end
        if (ok) begin
            req_level = req;
            #(($urandom % 60) + 1);
        end
    endtask

    int unsigned frame_hdr_bad = 0;   // a header arrived with frame low
    int unsigned frame_hdr_torn = 0;  // of those, torn by the tear test
    bit          last_ok = 1'b0;   // the copy of the last answer is valid
    logic [7:0]  last_hd = '0;
    int unsigned last_a = 0;
    int unsigned last_w = 0;
    logic [7:0]  last_st = '0;
    int unsigned last_rd = 0;
    int unsigned replays = 0;
    int unsigned rp_timeouts = 0;
    int unsigned ack_fixes = 0;
    int unsigned inj_mode = 0;     // 0 none, 1 drop the answer, 2 wrong SEQ, 3 stall after the header,
                                   // 4 do the access, then lose the answer
    int unsigned inj_every = 0;
    int unsigned inj_n = 0;
    int unsigned injected = 0;

    // As the Pico firmware does: release uio, wait until REQ has been
    // still for RP_TO; if the last REQ change went unanswered (ACK differs
    // from REQ), wait for the middle of the chip's quiet time (RP_FIX
    // after the last REQ change: 1.5 chip timeouts) and set ACK equal to
    // REQ there.
    localparam int RP_FIX = 30700;   // 1.5 x 1023 clocks at 50 MHz

    task automatic rp_resync();
        int   t;
        logic l;
        rp_timeouts++;
        rp_oe = 1'b0;
        #2;
        pdrv = 1'b0;
        l = req;
        t = 0;
        forever begin
            if (req !== l) begin
                l = req;
                t = 0;
            end
            if (pi_ack == l) begin
                if (t >= RP_TO) begin
                    break;
                end
            end else if (t >= RP_FIX) begin
                pi_ack = l;
                ack_fixes++;
                break;
            end
            #1;
            t++;
        end
        req_level = l;
    endtask

    // The PIO answers an event by setting ACK to REQ's new level (not by
    // toggling it), so this model does the same.
    task automatic toggle_ack();
        pi_ack = req_level;
    endtask

    task automatic drive(input logic [7:0] b);
        if (!rp_oe) begin
            if (dbg_do) begin
                $fatal(1, "RP: the chip still claims uio at %0t", $time);
            end
            pdrv = 1'b1;
            #1;
        end
        rp_out = b;
        rp_oe  = 1'b1;
        #4;
        toggle_ack();
    endtask

    initial begin : rp
        logic [7:0]  hd;
        int unsigned a;
        int unsigned w;
        int unsigned size;
        logic [7:0]  st;
        int unsigned rd;
        int unsigned n;
        int unsigned inj;
        bit          ok;
        bit          fail;
        forever begin
            wait (pi_rst_n);
            wait_req();          // header; no timeout while idle
            hd = uio;
            if (!dut.frame_d) begin
                frame_hdr_bad++;
            end
            inj_n++;
            inj = (inj_mode != 0 && inj_every != 0 && (inj_n % inj_every) == 0) ? inj_mode : 0;
            toggle_ack();
            fail = 1'b0;
            if (inj == 3) begin  // stall: take the header, then nothing
                injected++;
                #(RP_TO);
                fail = 1'b1;
            end
            size = (hd[5:4] == 0) ? 1 : (hd[5:4] == 1) ? 2 : (hd[5:4] == 2) ? 4 : 0;
            a = 0;
            if (!fail && !hd[6]) begin
                for (int i = 0; i < 4; i++) begin
                    wait_req_to(ok);
                    if (!ok) begin
                        fail = 1'b1;
                        break;
                    end
                    a[8 * i +: 8] = uio;
                    toggle_ack();
                end
            end
            w = 0;
            if (!fail && hd[7]) begin
                for (int i = 0; i < size; i++) begin
                    wait_req_to(ok);
                    if (!ok) begin
                        fail = 1'b1;
                        break;
                    end
                    w[8 * i +: 8] = uio;
                    toggle_ack();
                end
            end
            if (!fail) begin
                wait_req_to(ok);  // handoff
                fail = !ok;
            end
            if (!fail && inj == 1) begin  // drop: no answer, nothing done
                injected++;
                #(RP_TO);
                fail = 1'b1;
            end
            if (fail) begin
                rp_resync();
                continue;
            end
            if (last_ok && hd == last_hd && a == last_a && w == last_w) begin
                st = last_st;   // a repeat: the same answer, no second side effect
                rd = last_rd;
                replays++;
            end else begin
                access(hd, a, w, st, rd);
                last_ok = 1'b1;
                last_hd = hd;
                last_a  = a;
                last_w  = w;
                last_st = st;
                last_rd = rd;
            end
            n = (st == 0 && !hd[7]) ? size : 0;
            if (inj == 4) begin   // done, but the answer never arrives
                injected++;
                #(RP_TO);
                rp_resync();
                continue;
            end
            if (inj == 2) begin
                injected++;
                drive({~hd[0], hd[0], 3'b000, st[2:0]});   // wrong SEQ
            end else begin
                drive({hd[0], ~hd[0], 3'b000, st[2:0]});
            end
            for (int i = 0; i < n; i++) begin
                wait_req_to(ok);
                if (!ok) begin
                    fail = 1'b1;
                    break;
                end
                drive(rd[8 * i +: 8]);
            end
            if (!fail) begin
                wait_req_to(ok);  // last byte taken
                fail = !ok;
            end
            if (fail) begin
                rp_resync();
                continue;
            end
            rp_oe = 1'b0;
            #2;
            pdrv = 1'b0;
            #2;
            toggle_ack();  // handback
        end
    end

    initial begin : irq_gen
        forever begin
            #(20000 + $urandom % 80000);
            if (irq_on && !pi_irq) begin
                irq_t  = $time;
                pi_irq = 1'b1;
            end
        end
    end

    /* Test sequence */

    int errors = 0;

    task automatic check(input bit ok, input string what);
        if (!ok) begin
            $display("FAIL: %s", what);
            errors++;
        end
    endtask

    task automatic man(input logic [7:0] hd, input logic [23:0] a, input logic [31:0] w,
                       input logic [7:0] est, input logic [31:0] erd, input string name);
        logic prev_done;
        prev_done = get(0, 1)[0];
        put(24, 8, 64'(hd));
        put(32, 24, 64'(a));
        put(56, 32, 64'(w));
        put(0, 1, 64'(~src[0]));
        wait (get(0, 1)[0] != prev_done);
        check(get(1, 8) == 64'(est) && get(9, 32) == 64'(erd),
              $sformatf("manual %s: got %02x %08x, want %02x %08x", name,
                        get(1, 8), get(9, 32), est, erd));
    endtask

    task automatic stop_run();
        put(1, 1, 0);
        wait (get(41, 1) == 0 && get(42, 1) == 1);
        #2000;
    endtask

    task automatic report(input string name);
        $display("%s: tx %0d err %0d viol %0d ack glitches %0d max tx %0d clk", name,
                 get(43, 32), get(75, 32), get(304, 16), get(368, 32), get(428, 32));
        $display("  RAM rd %0d wr %0d fetch %0d next %0d  flash rd %0d fetch %0d  timer %0d scratch %0d",
                 count(0), count(1), count(2), count(3), count(4), count(5), count(6), count(7));
        $display("  console %0d disk %0d fault %0d fill %0d irq %0d sectors %0d",
                 count(8), count(9), count(10), count(11), count(12), count(13));
        if (get(195, 1)) begin
            $display("  first error kind %0d status %02x hdr %02x addr %06x want %08x got %08x",
                     get(196, 4), get(200, 8), get(296, 8), get(208, 24), get(232, 32), get(264, 32));
        end
    endtask

    initial begin : main
        logic [63:0] tx0;
        put(2, 1, 1);           // stop on the first error
        put(16, 4, 2);          // 10 to 15 ns in
        put(20, 4, 2);          // and out
        put(88, 32, 64'(SEED));
        put(120, 32, 64'(FSEED));
        put(152, 16, 512);      // RAM words
        put(168, 18, 64'(FLASH_WIN / 4));
        put(186, 4, 3);         // runs of 8
        dut.u_issp_c.src_v = 26'(3) << 17;   // chip clock 50 MHz
        #3000;
        pi_rst_n = 1'b1;
        con_t0   = now_us();
        #2000;
        check(get(320, 16) == 1, "one reset at start");

        man(8'h24, 24'hFF0000, 0, 8'h00, DEV_ID, "read ID");
        man(8'h24, 24'h800010, 0, 8'h00, h(32'h10, FSEED), "flash word");
        man(8'h14, 24'h800012, 0, 8'h00, h(32'h10, FSEED) >> 16, "flash halfword");
        man(8'hA4, 24'h800000, 0, 8'h01, 0, "flash write");
        man(8'h2C, 24'h800020, 0, 8'h00, h(32'h20, FSEED), "flash fetch");
        man(8'h6C, 24'h000000, 0, 8'h00, h(32'h24, FSEED), "flash fetch NEXT");

        // Mixed run without the disk, delays on.
        irq_on = 1;
        tx0 = 64'(rp_tx);
        put(4, 12, 64'hDFF);
        put(1, 1, 1);
        #4000000;
        stop_run();
        report("mixed, 10-15 ns pin delays");
        check(get(75, 32) == 0 && get(304, 16) == 0, "mixed run clean");
        check(64'(rp_tx) - tx0 == get(43, 32), "transaction counts match");
        for (int k = 0; k < 13; k++) begin
            if (k != 9) begin
                check(count(k) != 0, $sformatf("transactions of kind %0d", k));
            end
        end

        // Disk and IRQ only, until two sectors have come back.
        put(3, 1, 64'(~src[3]));
        put(4, 12, 64'hA00);
        put(1, 1, 1);
        fork
            wait (count(13) >= 2);
            #40000000;
        join_any
        stop_run();
        report("disk");
        $display("  dcnt %0d gen %0d op %0d t %0d", dut.dcnt_q, dut.gen_q, dut.op_q, dut.t_q);
        $display("  disk state %0d  model done %0d busy %0d err %0d sec %0d buf %06x cmd %0d",
                 get(417, 4), disk_done, disk_busy, disk_err, disk_sec, disk_buf, disk_cmd);
        check(get(75, 32) == 0 && count(13) >= 2, "two disk sectors verified");

        // Everything, no delays.
        put(3, 1, 64'(~src[3]));
        put(16, 4, 0);
        put(20, 4, 0);
        put(4, 12, 64'hFFF);
        put(1, 1, 1);
        #2000000;
        stop_run();
        report("mixed, no pin delays");
        check(get(75, 32) == 0, "mixed run without delays clean");

        // The same at 25 MHz with the longest delays: 75 to 80 ns each way.
        pi_rst_n = 1'b0;
        #2000;
        req_level = 1'b0;  // the chip leaves reset with REQ low
        pi_ack    = 1'b0;  // the RP restarts its server with ACK low
        last_ok   = 1'b0;  // and with SEQ 0
        clk_half = 20.0;
        dut.u_issp_c.src_v = 26'(6) << 17;   // chip clock 25 MHz
        put(16, 4, 15);
        put(20, 4, 15);
        put(152, 16, 64);       // a short fill so the run gets to the mix
        #2000;
        pi_rst_n = 1'b1;
        #4000;
        put(3, 1, 64'(~src[3]));
        put(1, 1, 1);
        #2000000;
        stop_run();
        report("mixed, 25 MHz, 75-80 ns pin delays");
        check(get(75, 32) == 0, "mixed run with long delays clean");

        // Link tests, 50 MHz, 10-15 ns delays, full mix.
        pi_rst_n = 1'b0;
        #2000;
        req_level = 1'b0;
        pi_ack    = 1'b0;
        last_ok   = 1'b0;
        clk_half  = 10.0;
        dut.u_issp_c.src_v = 26'(3) << 17;   // chip clock 50 MHz
        put(16, 4, 2);
        put(20, 4, 2);
        put(152, 16, 32);
        #2000;
        pi_rst_n = 1'b1;
        #4000;
        check(link(0) == 0 && link(1) == 0 && link(2) == 0 && link(3) == 0, "no link fault so far");

        // 1. ACK glitches of 30 ns (one or two samples at 50 MHz) at
        //    random times: the three-sample filter rejects every one.
        put(3, 1, 64'(~src[3]));
        put(1, 1, 1);
        tx0 = 64'(get(368, 32));
        for (int i = 0; i < 300; i++) begin
            #(1000 + $urandom % 3000);
            ack_gl = 1'b1;
            #30;
            ack_gl = 1'b0;
        end
        stop_run();
        report("link: 300 ACK glitches of 30 ns");
        $display("  ACK glitches counted %0d, link faults %0d %0d %0d, LINK_ERROR %0d",
                 64'(get(368, 32)) - tx0, link(0), link(1), link(2), link(3));
        check(get(75, 32) == 0 && link(0) == 0 && link(1) == 0 && link(2) == 0 && link(3) == 0,
              "ACK glitches: no error, no link fault");

        // 2. RP faults, one in every 7 transactions: drop the answer,
        //    answer with the wrong SEQ, stall after the header, lose the
        //    answer after doing the access. Modes 2 and 4 must be answered
        //    from the RP's copy, so writes and console reads happen once.
        for (int m = 1; m <= 4; m++) begin
            int unsigned f0 [4];
            int unsigned i0;
            int unsigned r0;
            for (int k = 0; k < 4; k++) begin
                f0[k] = link(k);
            end
            i0 = injected;
            r0 = replays;
            inj_mode  = m;
            inj_every = 7;
            put(3, 1, 64'(~src[3]));
            put(1, 1, 1);
            #1500000;
            inj_mode = 0;
            #200000;
            stop_run();
            report($sformatf("link: RP fault mode %0d", m));
            $display("  injected %0d, timeouts %0d, bad answers %0d, bad states %0d, LINK_ERROR %0d, replays %0d",
                     injected - i0, link(0) - f0[0], link(1) - f0[1], link(2) - f0[2], link(3) - f0[3],
                     replays - r0);
            check(get(75, 32) == 0, $sformatf("mode %0d: no data error", m));
            check(link(3) == f0[3], $sformatf("mode %0d: no LINK_ERROR", m));
            check(injected - i0 >= 20, $sformatf("mode %0d: faults were injected", m));
            if (m == 2) begin
                check(link(1) - f0[1] == injected - i0 && link(0) == f0[0],
                      "mode 2: every wrong SEQ is one bad-answer fault");
                check(replays - r0 == injected - i0, "mode 2: every retry answered from the copy");
            end else if (m == 4) begin
                check(link(0) - f0[0] == injected - i0 && link(1) == f0[1],
                      "mode 4: every lost answer is one timeout");
                check(replays - r0 == injected - i0, "mode 4: every retry answered from the copy");
            end else begin
                check(link(0) - f0[0] == injected - i0 && link(1) == f0[1],
                      $sformatf("mode %0d: every fault is one timeout", m));
                check(replays == r0, $sformatf("mode %0d: nothing answered from the copy", m));
            end
        end

        // 3. A scrambled state register (the unused code) mid-transaction.
        begin
            int unsigned s0;
            int unsigned n_inj;
            s0    = link(2);
            n_inj = 0;
            put(3, 1, 64'(~src[3]));
            put(1, 1, 1);
            while (n_inj < 20) begin
                #(20000 + $urandom % 20000);
                @(negedge dut.clk);
                if (dut.u_bus.state_q == 3'd2 || dut.u_bus.state_q == 3'd4) begin
                    force dut.u_bus.state_q = 3'd7;
                    #1;
                    release dut.u_bus.state_q;
                    n_inj++;
                end
            end
            #200000;
            stop_run();
            report("link: 20 scrambled states");
            $display("  bad states %0d, timeouts %0d, LINK_ERROR %0d", link(2) - s0, link(0), link(3));
            check(get(75, 32) == 0 && link(2) - s0 == 20 && link(3) == 0,
                  "scrambled states: each one recovered");
        end

        // 3b. Torn into IDLE by the design's tear test, the way the board
        //     shows it: 20 tears in each of the four states, every 8th
        //     transaction, each exactly one bad-state fault and a retry.
        for (int t = 0; t < 4; t++) begin
            int unsigned s0, n0, b0, o0, h0;
            h0 = frame_hdr_bad;
            s0 = link(2);
            n0 = tears();
            b0 = link(0) + link(1) + link(3);
            o0 = replays;
            put(191, 2, 64'(t));
            put(193, 4, 3);
            put(190, 1, 1);
            put(3, 1, 64'(~src[3]));
            put(1, 1, 1);
            wait (tears() - n0 >= 20);
            #20000;
            stop_run();
            put(190, 1, 0);
            report($sformatf("link: tears in state %0d", t == 3 ? 5 : t + 1));
            $display("  tears %0d, bad states %0d, other link faults %0d, replays %0d",
                     tears() - n0, link(2) - s0, link(0) + link(1) + link(3) - b0, replays - o0);
            check(get(75, 32) == 0, $sformatf("tears in state %0d: no data error", t == 3 ? 5 : t + 1));
            check(tears() - n0 >= 20 && link(2) - s0 == tears() - n0,
                  $sformatf("tears in state %0d: each one is one bad-state fault", t == 3 ? 5 : t + 1));
            check(link(0) + link(1) + link(3) == b0,
                  $sformatf("tears in state %0d: no other link fault, no LINK_ERROR", t == 3 ? 5 : t + 1));
            // A tear on the header's own SEND_TOGGLE, or in the SEND_WAIT
            // right after it: frame falls one or two clocks after the
            // header's REQ change as the bus unit gives up, so the RP,
            // which sees REQ after its debounce, may take that header with
            // frame already low. Only in those two states.
            if (t < 2) begin
                frame_hdr_torn += frame_hdr_bad - h0;
                $display("  headers taken with frame low (torn just after the header) %0d", frame_hdr_bad - h0);
                check(frame_hdr_bad - h0 <= tears() - n0,
                      $sformatf("tears in state %0d: at most one torn header each", t + 1));
            end else begin
                check(frame_hdr_bad == h0, $sformatf("tears in state %0d: no header with frame low",
                                                     t == 3 ? 5 : t + 1));
            end
        end

        // 4. A dead RP: every attempt is dropped. After four attempts the
        //    chip gives up with LINK_ERROR (status 5) instead of hanging,
        //    and works again as soon as the RP answers.
        begin
            int unsigned e0;
            e0 = link(3);
            put(2, 1, 0);       // keep running past errors
            inj_mode  = 1;
            inj_every = 1;
            put(3, 1, 64'(~src[3]));
            put(1, 1, 1);
            #400000;
            inj_mode = 0;
            #100000;
            stop_run();
            report("link: dead RP");
            $display("  LINK_ERROR %0d", link(3) - e0);
            check(link(3) - e0 >= 2 && get(75, 32) == link(3) - e0,
                  "dead RP: each transaction ends with LINK_ERROR, nothing else fails");
            put(2, 1, 1);
            put(3, 1, 64'(~src[3]));
            put(1, 1, 1);
            #600000;
            stop_run();
            report("link: RP back");
            check(get(75, 32) == 0 && get(43, 32) > 100, "RP back: clean run");
        end

        $display("RP: tx %0d faults %0d console echoed %0d bad %0d irq %0d max latency %0d ns disk ops %0d",
                 rp_tx, rp_faults, con_echo, con_bad, irq_count, irq_lat_max, disk_done);
        $display("RP: replays %0d recoveries %0d ACK repairs %0d injected %0d", replays, rp_timeouts,
                 ack_fixes, injected);
        check(con_bad == 0 && con_echo != 0, "console echo");
        check(irq_count != 0, "interrupts answered");
        check(get(304, 16) == 0, "no safety-net violations");
        check(count(14) == 0 && count(15) == 0, "no overlap events");
        $display("frame: rises %0d, balance %0d, REQ outside frame %0d, uio_oe outside frame %0d, header with frame low %0d",
                 dut.probe_c[383:352], dut.probe_c[415:384], dut.probe_c[431:416],
                 dut.probe_c[447:432], frame_hdr_bad);
        check(dut.probe_c[383:352] > 1000, "frame: rises counted");
        check(dut.probe_c[415:384] == 0, "frame: every rise is a start or a retry");
        check(dut.probe_c[431:416] == 0, "frame: every REQ change inside frame, at the pins");
        check(dut.probe_c[447:432] == 0, "frame: uio_oe only inside frame, at the pins");
        check(frame_hdr_bad == frame_hdr_torn,
              "frame: high at every header the RP takes, but for headers torn by the tear test");

        // The overlap counters: with the bus idle, hold this side's enable
        // on and pulse DBG_DI. 2 ns pulses (crosstalk) may count as events
        // but never as long ones; a 40 ns pulse (a real overlap) is long.
        force dut.oe_d = 1'b1;
        #100;
        for (int i = 0; i < 20; i++) begin
            pdrv = 1'b1;
            #2;
            pdrv = 1'b0;
            #(100 + $urandom % 7);
        end
        $display("overlap counters after 20 pulses of 2 ns: events %0d long %0d", count(14), count(15));
        check(count(14) >= 1 && count(14) <= 20 && count(15) == 0, "short pulses are not long overlaps");
        tx0 = 64'(count(14));
        pdrv = 1'b1;
        #40;
        pdrv = 1'b0;
        #100;
        $display("after one pulse of 40 ns: events %0d long %0d", count(14), count(15));
        check(64'(count(14)) == tx0 + 1 && count(15) == 1, "a 40 ns overlap is one long event");
        release dut.oe_d;

        // CLK integrity monitor: a steady 25 MHz CLK is 8 fast cycles a
        // period and gives no anomaly; one runt high pulse in a low half
        // shows as short periods.
        clk_half = 20.0;
        dut.u_issp_c.src_v = {1'b0, 8'd6, 8'd9, 8'd7, 1'b0};
        #400;
        dut.u_issp_c.src_v = {1'b0, 8'd6, 8'd9, 8'd7, 1'b1};
        #4000;
        $display("CLK monitor, steady: periods %0d short %0d long %0d min %0d max %0d high min %0d low min %0d",
                 dut.probe_c[31:0], dut.probe_c[63:32], dut.probe_c[95:64], dut.probe_c[103:96],
                 dut.probe_c[111:104], dut.probe_c[119:112], dut.probe_c[127:120]);
        check(dut.probe_c[31:0] >= 90 && dut.probe_c[63:32] == 0 && dut.probe_c[95:64] == 0
              && dut.probe_c[103:96] == 8 && dut.probe_c[111:104] == 8, "steady CLK has no anomaly");
        @(negedge pi_clk);
        #8;
        force dut.pi_clk = 1'b1;
        #2;
        release dut.pi_clk;
        #400;
        $display("CLK monitor, one runt: periods %0d short %0d long %0d min %0d max %0d high min %0d",
                 dut.probe_c[31:0], dut.probe_c[63:32], dut.probe_c[95:64], dut.probe_c[103:96],
                 dut.probe_c[111:104], dut.probe_c[119:112]);
        check(dut.probe_c[63:32] >= 1 && dut.probe_c[159:128] != 0, "a runt on CLK is caught");
        if (errors != 0) begin
            $fatal(1, "FAIL: %0d checks", errors);
        end
        $display("PASS");
        $finish;
    end
endmodule

// Simulation of the chip's own bus files (asc_bus, asc_reset_filter, asc_irq_filter) at
// their pins, against a behavioral RP that follows bus protocol v1
// (asc2-kb/bus-protocol-v1.md): it echoes the sequence bit, answers a
// repeat from its copy of the last answer, sets ACK to REQ's level the
// way the PIO does, and recovers from faults.
//
// Checked for every transaction:
// * the RP receives exactly the header, address and write data issued,
//   with the sequence bit the chip must send;
// * the chip receives exactly the status and read data the RP sent;
// * every access happens at the RP exactly once, retries included;
// * the chip and the RP never drive uio at the same time.
// Checked all the time, on the pins:
// * frame: every REQ change and every clock of uio_oe falls inside
//   frame; frame is low when done pulses; every rise of frame is a start
//   or a retry (rises = starts + link faults - LINK_ERRORs);
// * IRQ: a crosstalk pulse never reaches irq; a real level always does,
//   within 15 clocks even with crosstalk on it;
// * RST_N: a pulse shorter than 8 clocks never resets the chip.
//
// Phases:
//   1 crosstalk on ACK, RST_N and IRQ, no faults: no link fault at all
//   2 RP faults, one kind at a time: one link fault per injection
//   3 a scrambled state register: the unused code, then IDLE in the
//     middle of a transaction (a torn state change), with the timer RP
//     and the frame RP: one link fault each
//   4 phase 2 again with an RP that uses frame instead of its timers
//   5 an RP that drops every answer: LINK_ERROR, never a hang, then
//     clean again
//   6 resets in the middle of transactions: clean again each time
//
// make gl runs the same test on the gates Yosys makes from the three
// chip files (the synthesis tool of the Tiny Tapeout flow).
//
// TO_BITS is the chip's timeout width (16 in the chip; 10 here by
// default so that faults resolve quickly). The RP's own times scale with
// it as in the firmware rules: T_rp = 1/16 of the chip timeout, the ACK
// repair at 1.5 chip timeouts after the last REQ change.
module asc_bus_tb #(
    parameter int TO_BITS = 10,
    parameter int N_CLEAN = 20000,   // transactions in phase 1
    parameter int N_FAULT = 2100     // transactions per fault kind
);
    localparam real T_MAX   = 20.9;  // longest clock period, ns
    localparam real RP_TO   = (2.0 ** (TO_BITS - 4)) * 20.0;
    localparam real RP_FIX  = 1.5 * (2.0 ** TO_BITS - 1.0) * 20.0;
    localparam real CHIP_TO = (2.0 ** TO_BITS - 1.0) * T_MAX;

    typedef struct packed {
        logic [7:0]  hdr;
        logic [31:0] addr;
        logic [31:0] wdata;
        logic [7:0]  status;
        logic [31:0] rdata;
    } acc_t;

    logic        clk = 1'b0;
    logic        rst_n = 1'b0;
    logic        rst;
    logic        rst_raw;
    logic        start = 1'b0;
    logic        idle;
    logic [7:0]  hdr = '0;
    logic [31:0] addr = '0;
    logic [31:0] wdata = '0;
    logic        done;
    logic [7:0]  status;
    logic [31:0] rdata;
    logic [7:0]  chip_out;
    logic        chip_oe;
    logic        req;
    logic        frame;
    logic        pi_ack = 1'b0;
    logic        ack_xtalk = 1'b0;
    logic        rst_xtalk = 1'b0;
    logic        irq_level = 1'b0;
    logic        irq_xtalk = 1'b0;
    logic        irq;
    logic        ack_glitch;
    logic        fault;
    logic [1:0]  fault_why;
    /* verilator lint_off UNUSEDSIGNAL */
    logic [15:0] dbg;
    /* verilator lint_on UNUSEDSIGNAL */
    logic [7:0]  rp_out = '0;
    logic        rp_oe = 1'b0;
    logic [7:0]  bus;

    // 48 to 53 MHz with jitter, so the two sides never lock.
    always begin
        #(9.5 + ($urandom % 10) * 0.1) clk = ~clk;
    end

    // The devkit RP keeps pull-ups on uio: an undriven bus reads 0xFF.
    always_comb begin
        if (chip_oe) begin
            bus = chip_out;
        end else if (rp_oe) begin
            bus = rp_out;
        end else begin
            bus = 8'hFF;
        end
    end

    asc_reset_filter u_rst (
        .clk         (clk),
        .rst_n_async (rst_n ^ rst_xtalk),
        .rst         (rst),
        .rst_raw     (rst_raw)
    );

    asc_irq_filter u_irq (
        .clk       (clk),
        .irq_async (irq_level ^ irq_xtalk),
        .irq       (irq)
    );

    // Bench-only tear input of asc_bus (make tear): loads IDLE into the
    // state register for one clock, the torn edge on demand.
    logic tear = 1'b0;

    // GATE_LEVEL: the Yosys netlist (make gl), synthesized with TO_BITS
    // fixed, so the instance takes no parameter.
`ifdef GATE_LEVEL
    asc_bus dut (
`else
    asc_bus #(
        .TO_BITS (TO_BITS)
    ) dut (
`endif
        .clk        (clk),
        .rst        (rst),
        .start      (start),
        .idle       (idle),
        .hdr        (hdr),
        .addr       (addr),
        .wdata      (wdata),
        .done       (done),
        .status     (status),
        .rdata      (rdata),
        .uio_in     (bus),
        .uio_out    (chip_out),
        .uio_oe     (chip_oe),
        .req        (req),
        .frame      (frame),
        .ack_async  (pi_ack ^ ack_xtalk),
`ifdef ASC_BUS_TEST_TEAR
        .test_tear  (tear),
`endif
        .ack_glitch (ack_glitch),
        .fault      (fault),
        .fault_why  (fault_why),
        .dbg        (dbg)
    );

    int errors = 0;

    task automatic check(input bit ok, input string what);
        if (!ok) begin
            errors++;
            $display("FAIL: %s", what);
        end
    endtask

    /* Pin monitors */

    int unsigned frame_req_bad = 0;    // REQ changed while frame was low
    int unsigned frame_oe_bad = 0;     // uio_oe high while frame was low
    int unsigned frame_done_bad = 0;   // done while frame was high

    always begin
        #1;
        if (chip_oe && rp_oe) begin
            $fatal(1, "contention: chip and RP both drive uio at %0t", $time);
        end
        if (chip_oe && !frame) begin
            frame_oe_bad++;
        end
    end

    // A reset clears REQ with frame low; only changes outside reset count.
    always @(req) begin
        if (!rst && !frame) begin
            frame_req_bad++;
        end
    end

    int unsigned n_ack_glitch = 0;
    int unsigned n_resets = 0;
    int unsigned n_starts = 0;
    int unsigned n_rises = 0;
    int unsigned n_to = 0;
    int unsigned n_bad = 0;
    int unsigned n_state = 0;
    int unsigned n_link = 0;
    logic        rst_prev = 1'b0;
    logic        frame_prev = 1'b0;

    always @(posedge clk) begin
        rst_prev   <= rst;
        frame_prev <= frame;
        if (ack_glitch) begin
            n_ack_glitch++;
        end
        if (rst && !rst_prev) begin
            n_resets++;
        end
        if (!rst && start && idle) begin
            n_starts++;
        end
        if (frame && !frame_prev) begin
            n_rises++;
        end
        if (fault) begin
            case (fault_why)
                2'd1:    n_to++;
                2'd2:    n_bad++;
                default: n_state++;
            endcase
        end
        if (done) begin
            if (status == 8'd5) begin
                n_link++;
            end
            if (frame) begin
                frame_done_bad++;
            end
        end
    end

    /* Crosstalk */

    bit          xtalk_on = 1'b0;
    int unsigned ack_pulses = 0;
    int unsigned rst_pulses = 0;
    int unsigned irq_pulses = 0;

    // ACK: pulses of 1 to 8 ns at random times.
    initial begin : ack_noise
        forever begin
            #(30 + $urandom % 400);
            if (xtalk_on) begin
                ack_xtalk = 1'b1;
                #(1 + $urandom % 8);
                ack_xtalk = 1'b0;
                ack_pulses++;
            end
        end
    end

    // RST_N: pulses of 5 to 110 ns, under 8 clocks.
    initial begin : rst_noise
        forever begin
            #(2000 + $urandom % 20000);
            if (xtalk_on) begin
                rst_xtalk = 1'b1;
                #(5 + $urandom % 106);
                rst_xtalk = 1'b0;
                rst_pulses++;
            end
        end
    end

    // IRQ: pulses of 1 to 30 ns (under two clocks, so at most two samples
    // see one), at least 100 ns apart, on top of a real level that changes
    // every 2 to 12 us.
    initial begin : irq_noise
        forever begin
            #(100 + $urandom % 500);
            if (xtalk_on) begin
                irq_xtalk = 1'b1;
                #(1 + $urandom % 30);
                irq_xtalk = 1'b0;
                irq_pulses++;
            end
        end
    end

    realtime     irq_t = 0;
    int unsigned irq_changes = 0;
    int unsigned irq_bad_glitch = 0;
    int unsigned irq_bad_late = 0;
    logic        irq_prev = 1'b0;

    initial begin : irq_source
        forever begin
            #(2000 + $urandom % 10000);
            irq_level = ~irq_level;
            irq_t     = $realtime;
            irq_changes++;
        end
    end

    // Six clocks from the pin to this check, plus up to one crosstalk
    // pulse spoiling the first samples: 15 clocks is a safe bound.
    always @(posedge clk) begin
        irq_prev <= irq;
        if ($realtime - irq_t > 15.0 * T_MAX) begin
            if (irq != irq_prev) begin
                irq_bad_glitch++;
            end
            if (irq != irq_level) begin
                irq_bad_late++;
            end
        end
    end

    /* RP model */

    bit          rp_frame = 1'b0;   // recover by watching frame, not timers
    int unsigned epoch = 0;         // bumped by every reset the RP applies
    int unsigned tx_epoch = 0;      // epoch of the transaction in hand: after
                                    // a reset, the RP drops it and touches
                                    // neither uio nor ACK for it
    logic        req_level = 1'b0;

    bit          last_ok = 1'b0;    // the copy of the last answer is valid
    acc_t        last;
    int unsigned replays = 0;
    int unsigned rp_resyncs = 0;
    int unsigned ack_fixes = 0;     // ACK set to REQ by the timer path
    int unsigned frame_fixes = 0;   // ACK set to REQ on frame low
    int unsigned frame_hdr_bad = 0; // a header arrived with frame low
    int unsigned accesses = 0;
    int unsigned inj_mode = 0;      // 0 none, 1 drop the answer, 2 wrong SEQ,
                                    // 3 stall after the header,
                                    // 4 do the access, then lose the answer
    int unsigned inj_every = 0;
    int unsigned inj_n = 0;
    int unsigned injected = 0;

    acc_t        sb[$];             // accesses done, oldest first

    // Waits for the header. No timeout while idle.
    task automatic wait_hdr(output bit ok);
        int unsigned e0;
        e0 = epoch;
        wait (req !== req_level || epoch != e0);
        ok = (req !== req_level) && epoch == e0 && rst_n;
        if (ok) begin
            tx_epoch  = e0;
            req_level = req;
            if (!frame) begin
                frame_hdr_bad++;
            end
            #(($urandom % 60) + 1);
        end
    endtask

    // Mid-transaction wait: gives up after RP_TO, or, in frame mode, as
    // soon as frame is low (the bus unit gave up).
    task automatic wait_req(output bit ok);
        realtime t0;
        t0 = $realtime;
        ok = 1'b0;
        forever begin
            if (epoch != tx_epoch) begin
                break;
            end
            if (req !== req_level) begin
                ok = 1'b1;
                break;
            end
            if (rp_frame ? !frame : ($realtime - t0 >= RP_TO)) begin
                break;
            end
            #1;
        end
        if (ok) begin
            req_level = req;
            #(($urandom % 60) + 1);
            ok = (epoch == tx_epoch);
        end
    endtask

    task automatic release_uio();
        rp_oe = 1'b0;
        #2;
    endtask

    // Firmware recovery, timer path: release uio, wait until REQ has been
    // still for RP_TO; if the last REQ change went unanswered (ACK
    // differs from REQ), wait until RP_FIX after it, the middle of the
    // chip's quiet time, and set ACK equal to REQ there.
    task automatic resync_timer();
        realtime     t0;
        logic        l;
        int unsigned e0;
        e0 = tx_epoch;
        if (epoch != e0) begin
            return;
        end
        rp_resyncs++;
        release_uio();
        l  = req;
        t0 = $realtime;
        forever begin
            if (epoch != e0) begin
                break;
            end
            if (req !== l) begin
                l  = req;
                t0 = $realtime;
            end
            if (pi_ack == l) begin
                if ($realtime - t0 >= RP_TO) begin
                    break;
                end
            end else if ($realtime - t0 >= RP_FIX) begin
                pi_ack = l;
                ack_fixes++;
                break;
            end
            #1;
        end
        req_level = l;
    endtask

    // Recovery, frame path: called only in the middle of a transaction,
    // so frame low means the bus unit is quiet (it goes idle only after
    // the RP's handback, or after a quiet time). Wait for that, then set
    // ACK to REQ's level, which the quiet bus unit takes as its baseline.
    // Never touch ACK while the bus unit may be idle.
    task automatic resync_frame();
        int unsigned e0;
        e0 = tx_epoch;
        if (epoch != e0) begin
            return;
        end
        rp_resyncs++;
        release_uio();
        wait (!frame || epoch != e0);
        if (epoch == e0) begin
            if (pi_ack != req) begin
                frame_fixes++;
            end
            pi_ack    = req;
            req_level = req;
        end
    endtask

    task automatic resync();
        if (rp_frame) begin
            resync_frame();
        end else begin
            resync_timer();
        end
    endtask

    // The PIO answers by setting ACK to REQ's new level.
    task automatic answer();
        if (epoch == tx_epoch) begin
            pi_ack = req_level;
        end
    endtask

    task automatic drive(input logic [7:0] b);
        if (epoch != tx_epoch) begin
            return;
        end
        if (!rp_oe && chip_oe) begin
            $fatal(1, "RP: the chip still drives uio at %0t", $time);
        end
        rp_out = b;
        rp_oe  = 1'b1;
        #4;
        answer();
    endtask

    function automatic int unsigned size_of(input logic [7:0] h);
        case (h[5:4])
            2'd0:    return 1;
            2'd1:    return 2;
            2'd2:    return 4;
            default: return 0;
        endcase
    endfunction

    initial begin : rp
        acc_t        x;
        int unsigned size;
        int unsigned n;
        int unsigned inj;
        bit          ok;
        bit          fail;
        forever begin
            wait (rst_n);
            wait_hdr(ok);
            if (!ok) begin
                continue;
            end
            x       = '0;
            x.hdr   = bus;
            inj_n++;
            inj = (inj_mode != 0 && inj_every != 0 && (inj_n % inj_every) == 0) ? inj_mode : 0;
            answer();
            fail = 1'b0;
            if (inj == 3) begin
                injected++;
                #(RP_TO);
                fail = 1'b1;
            end
            size = size_of(x.hdr);
            if (!fail && !x.hdr[6]) begin
                for (int i = 0; i < 4; i++) begin
                    wait_req(ok);
                    if (!ok) begin
                        fail = 1'b1;
                        break;
                    end
                    x.addr[8 * i +: 8] = bus;
                    answer();
                end
            end
            if (!fail && x.hdr[7]) begin
                for (int i = 0; i < int'(size); i++) begin
                    wait_req(ok);
                    if (!ok) begin
                        fail = 1'b1;
                        break;
                    end
                    x.wdata[8 * i +: 8] = bus;
                    answer();
                end
            end
            if (!fail) begin
                wait_req(ok);   // handoff
                fail = !ok;
            end
            if (!fail && inj == 1) begin
                injected++;
                #(RP_TO);
                fail = 1'b1;
            end
            if (fail) begin
                resync();
                continue;
            end
            if (epoch != tx_epoch) begin
                continue;
            end
            if (last_ok && x.hdr == last.hdr && x.addr == last.addr && x.wdata == last.wdata) begin
                x = last;   // a repeat: the same answer, no second access
                replays++;
            end else begin
                x.status = ($urandom % 8 == 0) ? 8'(1 + $urandom % 4) : 8'h00;
                x.rdata  = $urandom;
                accesses++;
                sb.push_back(x);
                last    = x;
                last_ok = 1'b1;
            end
            n = (x.status == 8'h00 && !x.hdr[7]) ? size : 0;
            if (inj == 4) begin
                injected++;
                #(RP_TO);
                resync();
                continue;
            end
            if (inj == 2) begin
                injected++;
                drive({~x.hdr[0], x.hdr[0], 3'b000, x.status[2:0]});
            end else begin
                drive({x.hdr[0], ~x.hdr[0], 3'b000, x.status[2:0]});
            end
            for (int i = 0; i < int'(n); i++) begin
                wait_req(ok);
                if (!ok) begin
                    fail = 1'b1;
                    break;
                end
                drive(x.rdata[8 * i +: 8]);
            end
            if (!fail) begin
                wait_req(ok);   // last byte taken
                fail = !ok;
            end
            if (fail) begin
                resync();
                continue;
            end
            release_uio();
            answer();   // handback
        end
    end

    // A reset applied by the RP: the copy is cleared (firmware rule 3),
    // uio is released, ACK is set to the level REQ takes in reset.
    task automatic rp_reset(input realtime hold);
        epoch++;
        rst_n     = 1'b0;
        rp_oe     = 1'b0;
        last_ok   = 1'b0;
        #(hold);
        pi_ack    = 1'b0;
        req_level = 1'b0;
        #(hold);
        rst_n     = 1'b1;
    endtask

    /* Chip side: issue transactions and check every answer */

    logic        exp_seq = 1'b0;
    int unsigned seen_resets = 0;
    int unsigned done_ok = 0;
    int unsigned done_link = 0;
    int unsigned aborted = 0;
    int unsigned unknown = 0;   // LINK_ERROR after the access was done
    int unsigned data_errors = 0;

    function automatic logic [31:0] mask_of(input int unsigned sz);
        return (sz == 4) ? 32'hFFFFFFFF : ((32'h1 << (8 * sz)) - 32'h1);
    endfunction

    task automatic issue_one();
        logic [7:0]  h;
        logic [31:0] a;
        logic [31:0] w;
        int unsigned r0;
        int unsigned sz;
        acc_t        x;
        bit          got;
        realtime     t0;
        h  = 8'($urandom);
        h[1:0] = 2'b00;
        a  = $urandom;
        w  = $urandom;
        sz = size_of(h);
        @(negedge clk);
        while (rst || !idle) begin
            @(negedge clk);
        end
        if (n_resets != seen_resets) begin
            // The transaction a reset cut off may have been done at the
            // RP; nothing is outstanding now, and SEQ starts from 0.
            seen_resets = n_resets;
            sb.delete();
            exp_seq = 1'b0;
        end
        r0    = n_resets;
        hdr   = h;
        addr  = a;
        wdata = w;
        // The core's rule (asc_bus.sv): hold start until idle goes low,
        // then wait for idle high. Never rely on the done pulse.
        start = 1'b1;
        @(negedge clk);
        while (idle && n_resets == r0) begin
            @(negedge clk);
        end
        start = 1'b0;
        got   = 1'b0;
        t0    = $realtime;
        forever begin
            if (n_resets != r0) begin
                break;
            end
            if (idle) begin
                got = 1'b1;
                break;
            end
            // Four attempts take at most eight chip timeouts.
            if ($realtime - t0 > 12.0 * CHIP_TO) begin
                $display("FAIL: the bus unit never ended a transaction (started at %0t)", t0);
                $fatal(1, "asc_bus_tb: hang");
            end
            @(negedge clk);
        end
        if (!got) begin
            aborted++;
            return;
        end
        if (status == 8'd5) begin
            done_link++;
            // Completion unknown: the RP may have done the access and
            // lost every answer.
            if (sb.size() != 0 && sb[0].hdr == {h[7:2], 1'b0, exp_seq}
                && (h[6] || sb[0].addr == a)) begin
                void'(sb.pop_front());
                unknown++;
            end
            return;
        end
        done_ok++;
        if (sb.size() == 0) begin
            data_errors++;
            $display("ERROR %0t: done with no access at the RP", $time);
            return;
        end
        x = sb.pop_front();
        if (x.hdr != {h[7:2], 1'b0, exp_seq}
            || (!h[6] && x.addr != a)
            || (h[7] && ((x.wdata ^ w) & mask_of(sz)) != 0)
            || status != x.status
            || (!h[7] && x.status == 8'h00 && rdata != (x.rdata & mask_of(sz)))
            || ((h[7] || x.status != 8'h00) && rdata != 32'h0)) begin
            data_errors++;
            if (data_errors <= 10) begin
                $display("ERROR %0t: hdr %02x/%02x addr %08x/%08x wdata %08x/%08x status %02x/%02x rdata %08x/%08x",
                         $time, {h[7:2], 1'b0, exp_seq}, x.hdr, a, x.addr, w, x.wdata,
                         x.status, status, x.rdata, rdata);
            end
        end
        exp_seq = ~exp_seq;
    endtask

    task automatic issue(input int unsigned n);
        for (int unsigned i = 0; i < n; i++) begin
            issue_one();
        end
    endtask

    /* Phases */

    int unsigned p_to, p_bad, p_state, p_link, p_rises, p_starts, p_ok, p_lnk, p_err, p_inj,
                 p_rep, p_fix, p_ffix, p_acc;

    task automatic phase_begin();
        p_to     = n_to;
        p_bad    = n_bad;
        p_state  = n_state;
        p_link   = n_link;
        p_rises  = n_rises;
        p_starts = n_starts;
        p_ok     = done_ok;
        p_lnk    = done_link;
        p_err    = data_errors;
        p_inj    = injected;
        p_rep    = replays;
        p_fix    = ack_fixes;
        p_ffix   = frame_fixes;
        p_acc    = accesses;
    endtask

    // Counts since phase_begin; checks the frame balance when nothing is
    // in flight.
    task automatic phase_end(input string name, output int unsigned faults,
                             output int unsigned links);
        int unsigned rises, starts;
        faults = (n_to - p_to) + (n_bad - p_bad) + (n_state - p_state);
        links  = n_link - p_link;
        rises  = n_rises - p_rises;
        starts = n_starts - p_starts;
        $display("%s: %0d ok, %0d LINK_ERROR, %0d data errors; faults: %0d timeout, %0d bad answer, %0d bad state; injected %0d, replays %0d, ACK repairs %0d timer / %0d frame",
                 name, done_ok - p_ok, done_link - p_lnk, data_errors - p_err, n_to - p_to,
                 n_bad - p_bad, n_state - p_state, injected - p_inj, replays - p_rep,
                 ack_fixes - p_fix, frame_fixes - p_ffix);
        check(data_errors == p_err, {name, ": no data errors"});
        check(rises == starts + faults - links,
              $sformatf("%s: frame rises %0d = starts %0d + faults %0d - LINK_ERRORs %0d",
                        name, rises, starts, faults, links));
    endtask

    // Lets the last transaction finish and the RP settle.
    task automatic settle();
        while (!idle || rst) begin
            @(negedge clk);
        end
        #(3.0 * CHIP_TO);
    endtask

    initial begin : main
        int unsigned faults, links, r0, a0;
        $display("asc_bus_tb: TO_BITS %0d, chip timeout %0.1f us, RP_TO %0.2f us, ACK repair at %0.1f us",
                 TO_BITS, CHIP_TO / 1000.0, RP_TO / 1000.0, RP_FIX / 1000.0);
        #1000;
        rst_n = 1'b1;
        #1000;
        xtalk_on = 1'b1;

        // 1. Crosstalk only.
        phase_begin();
        issue(N_CLEAN);
        settle();
        phase_end("1 crosstalk", faults, links);
        check(faults == 0 && links == 0, "1: no link fault and no LINK_ERROR under crosstalk");
        check(n_ack_glitch > 0, "1: the ACK filter rejected some pulses");

        // 2. RP faults, one kind at a time, one in 7 headers.
        for (int unsigned m = 1; m <= 4; m++) begin
            phase_begin();
            inj_mode  = m;
            inj_every = 7;
            issue(N_FAULT);
            inj_mode  = 0;
            settle();
            phase_end($sformatf("2 fault mode %0d", m), faults, links);
            check(links == 0, $sformatf("2 mode %0d: no LINK_ERROR", m));
            check(injected - p_inj > 0 && faults == injected - p_inj,
                  $sformatf("2 mode %0d: one link fault per injection (%0d, %0d)", m, faults,
                            injected - p_inj));
            // Modes 2 and 4 come after the access: the retry is answered
            // from the copy. Modes 1 and 3 come before it.
            if (m == 2 || m == 4) begin
                check(replays - p_rep == injected - p_inj,
                      $sformatf("2 mode %0d: every retry answered from the copy", m));
            end else begin
                check(replays == p_rep, $sformatf("2 mode %0d: nothing answered from the copy", m));
            end
        end

        // 3. A scrambled state register (the unused code) mid-transaction.
        phase_begin();
        fork
            issue(N_FAULT);
            begin
                int unsigned k;
                k = 0;
                while (k < 20) begin
                    #(5000 + $urandom % 20000);
                    @(negedge clk);
                    if (dut.state_q == 3'd2 || dut.state_q == 3'd4) begin
                        force dut.state_q = 3'd7;
                        #1;
                        release dut.state_q;
                        k++;
                    end
                end
            end
        join
        settle();
        phase_end("3 scrambled state", faults, links);
        check(n_state - p_state == 20 && faults == 20 && links == 0,
              "3: each scrambled state cost one link fault, no LINK_ERROR");

        // 3b. IDLE in the middle of a transaction, in the send phase and in
        //     the receive phase (after the RP did the access: the retry
        //     must be answered from the copy). With the timer RP an
        //     unanswered REQ change can cost one more attempt (the retry
        //     comes before the ACK repair); with the frame RP it cannot.
        for (int fm = 0; fm < 2; fm++) begin
            rp_frame = (fm == 1);
            phase_begin();
            fork
                issue(N_FAULT);
                begin
                    int unsigned k;
                    k = 0;
                    while (k < 20) begin
                        #(5000 + $urandom % 20000);
                        @(negedge clk);
                        if (dut.state_q == 3'd2 || dut.state_q == 3'd4) begin
                            force dut.state_q = 3'd0;
                            #1;
                            release dut.state_q;
                            k++;
                        end
                    end
                end
            join
            settle();
            phase_end(fm ? "3b IDLE torn in, frame RP" : "3b IDLE torn in, timer RP", faults, links);
            check(n_state - p_state == 20 && links == 0,
                  $sformatf("3b %s: each torn IDLE caught as a bad state, no LINK_ERROR",
                            fm ? "frame RP" : "timer RP"));
            if (fm == 1) begin
                check(faults == 20, "3b frame RP: exactly one link fault each");
            end
        end
        rp_frame = 1'b0;

`ifdef ASC_BUS_TEST_TEAR
        // 3c. Torn edges through the tear input, as on the board: IDLE is
        //     loaded instead of the next state in SEND_TOGGLE (the edge the
        //     torture test tore), SEND_WAIT, HANDOFF and RECV_TOGGLE, 20
        //     each, with the timer RP and with the frame RP. Every REQ
        //     change before the tear was answered, so each tear must cost
        //     exactly one link fault with either RP. One tear per
        //     transaction.
        for (int fm = 0; fm < 2; fm++) begin
            rp_frame = (fm == 1);
            for (int t = 0; t < 4; t++) begin
                logic [2:0] target;
                target = (t == 3) ? 3'd5 : 3'(t + 1);
                phase_begin();
                fork
                    issue(N_FAULT);
                    begin
                        int unsigned k;
                        k = 0;
                        while (k < 20) begin
                            #(5000 + $urandom % 20000);
                            @(negedge clk);
                            while (dut.state_q != target) begin
                                @(negedge clk);
                            end
                            tear = 1'b1;
                            @(negedge clk);
                            tear = 1'b0;
                            k++;
                            // One tear per transaction, as on the board:
                            // four on the same one would rightly end it
                            // with LINK_ERROR.
                            while (!idle) begin
                                @(negedge clk);
                            end
                        end
                    end
                join
                settle();
                phase_end($sformatf("3c tear in state %0d, %s RP", target, fm ? "frame" : "timer"),
                          faults, links);
                check(n_state - p_state == 20 && faults == 20 && links == 0,
                      $sformatf("3c state %0d, %s RP: each tear one bad-state fault, no LINK_ERROR",
                                target, fm ? "frame" : "timer"));
            end
        end
        rp_frame = 1'b0;
`endif

        // 4. Phase 2 with an RP that watches frame instead of its timers.
        rp_frame = 1'b1;
        for (int unsigned m = 1; m <= 4; m++) begin
            phase_begin();
            inj_mode  = m;
            inj_every = 7;
            issue(N_FAULT);
            inj_mode  = 0;
            settle();
            phase_end($sformatf("4 frame RP, fault mode %0d", m), faults, links);
            check(links == 0, $sformatf("4 mode %0d: no LINK_ERROR", m));
            check(injected - p_inj > 0 && faults == injected - p_inj,
                  $sformatf("4 mode %0d: one link fault per injection (%0d, %0d)", m, faults,
                            injected - p_inj));
            check(ack_fixes == p_fix, $sformatf("4 mode %0d: the timer path was never used", m));
            if (m == 2 || m == 4) begin
                check(replays - p_rep == injected - p_inj,
                      $sformatf("4 mode %0d: every retry answered from the copy", m));
            end else begin
                check(replays == p_rep, $sformatf("4 mode %0d: nothing answered from the copy", m));
            end
        end
        rp_frame = 1'b0;

        // 5. An RP that drops every answer (the bench's dead RP): each
        //    transaction ends with LINK_ERROR after four attempts instead
        //    of hanging, and nothing is done twice; then clean.
        phase_begin();
        inj_mode  = 1;
        inj_every = 1;
        issue(20);
        inj_mode  = 0;
        settle();
        phase_end("5 dead RP", faults, links);
        check(links == 20 && faults == 80 && accesses == p_acc,
              "5: 20 transactions, 4 attempts each, then LINK_ERROR; no access done");
        phase_begin();
        issue(N_FAULT);
        settle();
        phase_end("5 after the dead RP", faults, links);
        check(faults == 0 && links == 0, "5: clean once the RP answers again");

        // 6. Resets in the middle of transactions.
        r0 = n_resets;
        a0 = aborted;
        fork
            issue(N_FAULT);
            begin
                for (int k = 0; k < 20; k++) begin
                    #(5000 + $urandom % 20000);
                    rp_reset(1000);
                end
            end
        join
        settle();
        $display("6 resets: %0d applied, %0d transactions cut off, %0d data errors so far",
                 n_resets - r0, aborted - a0, data_errors);
        check(n_resets - r0 == 20, "6: every applied reset took effect");
        phase_begin();
        issue(N_FAULT);
        settle();
        phase_end("6 after the resets", faults, links);
        check(faults == 0 && links == 0, "6: clean after the resets");

        xtalk_on = 1'b0;
        #1000;

        $display("pins: ACK pulses %0d (rejected %0d), RST_N pulses %0d, resets %0d, IRQ pulses %0d, IRQ changes %0d",
                 ack_pulses, n_ack_glitch, rst_pulses, n_resets, irq_pulses, irq_changes);
        $display("frame: rises %0d, starts %0d; REQ outside frame %0d, uio_oe outside frame %0d, done inside frame %0d, header with frame low %0d",
                 n_rises, n_starts, frame_req_bad, frame_oe_bad, frame_done_bad, frame_hdr_bad);
        $display("RP: %0d accesses, %0d replays, %0d resyncs, %0d completion unknown",
                 accesses, replays, rp_resyncs, unknown);
        check(n_resets == 21, "RST_N: only the 21 real resets took effect");
        check(frame_req_bad == 0, "frame: every REQ change inside frame");
        check(frame_oe_bad == 0, "frame: uio_oe only inside frame");
        check(frame_done_bad == 0, "frame: low when done pulses");
        check(frame_hdr_bad == 0, "frame: high at every header");
        check(irq_bad_glitch == 0, "IRQ: no crosstalk pulse reached irq");
        check(irq_bad_late == 0, "IRQ: every level reached irq within 15 clocks");
        check(irq_pulses > 1000 && irq_changes > 10, "IRQ: the test exercised the filter");

        if (errors == 0) begin
            $display("PASS");
            $finish;
        end else begin
            $display("FAILED: %0d checks", errors);
            $fatal(1, "asc_bus_tb failed");
        end
    end
endmodule

`timescale 1ns / 1ps

// Bench Test 4, DE10-Lite side: the chip's bus and reset RTL (src/ of the
// chip's repository, ttsky26e-almurut2) plus a test sequencer that uses
// every kind of memory and device the RP serves.
//
// Chip clock: 150 MHz / div from this board's PLL (div set over T4C,
// the same divisions and duty as the Pico's CLK). On the devkit the RP
// drives the chip clock over a short PCB trace with a 47 ohm resistor.
// On this bench the CLK jumper wire picked up extra edges whenever this
// side's pins switched (two ground wires carry all the return current),
// and an extra edge 5 to 15 ns after a real one corrupts state: the 25
// MHz hangs. The Pico still drives CLK on its wire at the same division;
// the CLK monitor (T4C) watches it. For the torture test, T4C source bit
// 25 puts the chip logic back on the CLK wire.
//
// Bus protocol v1 (asc2-kb/bus-protocol-v1.md): asc_bus filters ACK over
// three samples, checks every answer, times out, stays quiet and retries;
// T4C counts its link faults by kind. IRQ passes through asc_irq_filter, the
// chip's own synchronizer and filter. asc_bus's frame output (uo_out[3]
// on the chip) goes through the same output delays as REQ and is checked
// at that point the way a bridge would see it (T4C); it has no wire to
// the Pico on this bench.
//
// Wires (same pins and directions as Test 3):
//   pi_rst_n  in   RST_N
//   pi_clk    in   CLK from the Pico, watched by the CLK monitor only
//   uio[7:0]  bidir, driven only by asc_bus during its phase
//   req       out  REQ
//   pi_ack    in   ACK
//   pi_irq    in   IRQ, level, high while an interrupt is pending
//   pi_dbg_di in   bench safety net: high while the Pico drives uio
//   dbg_do    out  bench safety net: high while this side drives uio
//   clk50     in   the board's 50 MHz oscillator, for the pin delays only
//
// Pin delays (bench only): every chip input passes through dly_line with
// dly_in stages and every chip output with dly_out stages of 5 ns, like
// the Tiny Tapeout pads and multiplexer. 0 bypasses.
//
// Memory map used (served by bench/firmware):
//   0x000000-0x02FFFF  RAM, 192 KB; 0x02F000 and up is kernel-only
//     stress region from 0, ram_words words; disk buffers A 0x028000 and
//     B 0x029000 (the host keeps the region below A)
//   0x800000-          RP flash, read and fetch only, word at offset o
//                      holds h(o, fseed)
//   0xFF0000-0xFF002F  device registers, 4-byte data accesses only
//
// Host control over JTAG, In-System Sources and Probes.
// Instance "T4A" sources (200 bits, LSB first):
//   [0] man_go toggle   [1] stress_run   [2] stop_on_err   [3] clear toggle
//   [15:4] op mask      [19:16] dly_in   [23:20] dly_out
//   [31:24] manual hdr  [55:32] manual addr   [87:56] manual wdata
//   [119:88] seed (RAM data)   [151:120] fseed (flash image)
//   [167:152] ram_words  [185:168] flash_words  [189:186] log2 run length
//   [190] tear test on   [192:191] tear state: 0 SEND_TOGGLE, 1 SEND_WAIT,
//   2 HANDOFF, 3 RECV_TOGGLE   [196:193] log2 transactions per tear
//   [199:197] reserved
// Instance "T4A" probes (464 bits, LSB first):
//   [0] man_done toggle  [8:1] man status  [40:9] man rdata
//   [41] stress active   [42] bus idle     [74:43] transactions
//   [106:75] errors      [146:107] round trips (REQ toggles)
//   [194:147] clocks while stressing
//   [195] first error recorded  [199:196] its kind  [207:200] status
//   [231:208] address  [263:232] expected  [295:264] got  [303:296] header
//   [319:304] safety-net violations, fast clock cycles (never cleared)
//   [335:320] resets applied (never cleared)
//   [367:336] RST_N glitches rejected, 32 bits, wrapping (never cleared)
//   [399:368] ACK glitches rejected by asc_bus, 32 bits, wrapping (never
//             cleared); 16-bit saturating counters filled up in a soak
//   [415:400] asc_bus debug state   [427:416] sequencer state
//   [459:428] longest transaction, clocks   [463:460] reserved
// Instance "T4B" probes (496 bits): 14 transaction counters of 32 bits,
// by kind, then two safety-net overlap counters (never cleared):
//   0 RAM read, 1 RAM write, 2 RAM fetch, 3 RAM NEXT read, 4 flash read,
//   5 flash fetch, 6 timer, 7 scratch, 8 console, 9 disk, 10 fault,
//   11 RAM fill, 12 IRQ acknowledge, 13 disk sectors verified;
//   [479:448] overlap events that lasted 3 or more fast cycles (15 ns);
//   [495:480] overlap events, 16 bits, wrapping (fast cycles with this
//   side and the Pico both claiming uio, counted once per run of such
//   cycles). A glitch on the DBG_DI wire caught by the synchronizer
//   lasts 1 or 2 cycles; a real overlap lasts as long as the Pico
//   drives, 27 ns or more.
// Instance "T4C", chip clock and CLK wire integrity (bench only, fast
// clock domain). A flop toggled by every rising edge of the CLK wire
// (pi_clk) is sampled by the fast clock, so
// each CLK period shows as one toggle and its length in 5 ns steps. A
// runt or double edge that reaches the clock net shows as a period out
// of bounds: two toggles close together (short), or two toggles inside
// one fast cycle, seen as one long period. The pin itself is watched
// too: its shortest high and low levels.
//   sources (26 bits): [0] clear toggle  [8:1] lo  [16:9] hi; a period
//     below lo or above hi is an anomaly; [24:17] chip clock divider,
//     150 MHz / div (0 or 1 stops it; 6, 25 MHz, at configuration);
//     [25] chip clock source: 0 the divider, 1 the CLK wire (torture
//     test); change [25:17] only while RST_N is low
//   probes (352 bits): [31:0] periods  [63:32] short  [95:64] long
//     [103:96] shortest period  [111:104] longest period
//     [119:112] shortest high level  [127:120] shortest low level
//     [159:128] fast-clock time of the last anomaly
//     [191:160] fast-clock time of the last REQ change at the pin
//     [223:192] fast-clock time now
//     [255:224] asc_bus link timeouts  [287:256] bad answers
//     [319:288] bad states  [351:320] transactions ended with LINK_ERROR
//     [383:352] frame rises (32, wrapping)
//     [415:384] frame balance: rises - starts - link faults + LINK_ERRORs
//               (32, wrapping); unchanged by any run with the bus idle at
//               its end and no reset in the middle of a transaction
//     [431:416] REQ changes at the pins while frame was low (saturating)
//     [447:432] uio_oe at the pins while frame was low, two or more fast
//               cycles in a row (saturating)
//     [479:448] tears applied by the tear test (32, wrapping)
//
// Tear test (needs ASC_BUS_TEST_TEAR, set in the Quartus project and the
// Makefile): at every 2**n-th transaction a tear is armed; after a random
// wait of 0 to 255 chip clocks it fires on the first clock asc_bus is in
// the chosen state, and asc_bus loads IDLE instead of its next state for
// that clock: the torn clock edge the torture test found, on demand. In
// each of the four states the next state is another busy one on a clean
// link, so every tear must give exactly one bad-state fault and a retry.
//
// Op mask bits select what the stress mix may pick:
//   0 RAM read (1, 2 or 4 bytes)    1 RAM write (rewrites the same data)
//   2 RAM fetch run, NEXT           3 RAM data read run, NEXT
//   4 flash read                    5 flash fetch run, NEXT
//   6 timer (must not go back)      7 scratch write and read back
//   8 console echo                  9 disk: write a sector, read it back
//   10 expected faults              11 answer IRQ
// A run first fills the RAM region with h(address, seed), then picks
// random ops until stress_run drops, or until the first error when
// stop_on_err is set.
module bench_mem #(
    parameter int BUS_TO_BITS = 16   // asc_bus timeout and quiet time, 2**n - 1 clocks
) (
    input  logic       clk50,
    input  logic       pi_rst_n,
    input  logic       pi_clk,
    inout  logic [7:0] uio,
    output logic       req,
    input  logic       pi_ack,
    input  logic       pi_irq,
    // Unused in this test; declared so that its pin stays an input.
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic       pi_dbg_ck,
    /* verilator lint_on UNUSEDSIGNAL */
    input  logic       pi_dbg_di,
    output logic       dbg_do,
    output logic [9:0] LEDR,
    output logic [7:0] HEX0,
    output logic [7:0] HEX1,
    output logic [7:0] HEX2,
    output logic [7:0] HEX3,
    output logic [7:0] HEX4,
    output logic [7:0] HEX5
);
    localparam logic [23:0] FLASH_BASE  = 24'h800000;
    localparam logic [23:0] DEV_BASE    = 24'hFF0000;
    localparam logic [23:0] R_SCRATCH   = 24'hFF0004;
    localparam logic [23:0] R_TIMER     = 24'hFF000C;
    localparam logic [23:0] R_CON_STAT  = 24'hFF0010;
    localparam logic [23:0] R_CON_DATA  = 24'hFF0014;
    localparam logic [23:0] R_IRQ_ACK   = 24'hFF0018;
    localparam logic [23:0] R_DISK_SEC  = 24'hFF0020;
    localparam logic [23:0] R_DISK_BUF  = 24'hFF0024;
    localparam logic [23:0] R_DISK_CMD  = 24'hFF0028;
    localparam logic [23:0] R_DISK_STAT = 24'hFF002C;
    localparam logic [23:0] BUF_A       = 24'h028000;
    localparam logic [23:0] BUF_B       = 24'h029000;
    localparam logic [23:0] KPAGE       = 24'h02F000;

    localparam logic [7:0] ST_OK         = 8'h00;
    localparam logic [7:0] ST_BUS_ERROR  = 8'h01;
    localparam logic [7:0] ST_PROTECTION = 8'h02;
    localparam logic [7:0] ST_MISALIGNED = 8'h03;
    localparam logic [7:0] ST_BAD_NEXT   = 8'h04;

    /* Clocks, reset, host interface */

    logic clk;
    logic dclk;
    logic fclk;
    logic pll_locked;
    /* verilator lint_off UNUSEDSIGNAL */
    logic [4:0] pll_clk;   // clk[0] and clk[1] are used
    /* verilator lint_on UNUSEDSIGNAL */
    logic       ck_q;
    logic       ck_sel_m_q, ck_sel_q;   // 1: chip logic on the CLK wire (torture test)
    (* keep = 1 *) logic clk_mux;

    // Selected only while RST_N is low; a plain mux is enough then.
    always_comb begin
        clk_mux = ck_sel_q ? pi_clk : ck_q;
        clk  = clk_mux;
        dclk = pll_clk[0];
        fclk = pll_clk[1];
    end

    // 200 MHz for the pin delays: 50 MHz * 4. 150 MHz for the chip clock
    // divider: 50 MHz * 3.
    altpll #(
        .intended_device_family ("MAX 10"),
        .operation_mode         ("NORMAL"),
        .pll_type               ("AUTO"),
        .inclk0_input_frequency (20000),
        .clk0_multiply_by       (4),
        .clk0_divide_by         (1),
        .clk0_duty_cycle        (50),
        .clk0_phase_shift       ("0"),
        .clk1_multiply_by       (3),
        .clk1_divide_by         (1),
        .clk1_duty_cycle        (50),
        .clk1_phase_shift       ("0"),
        .compensate_clock       ("CLK0"),
        .width_clock            (5),
        .lpm_type               ("altpll")
    ) u_pll (
        .inclk  ({1'b0, clk50}),
        .clk    (pll_clk),
        .locked (pll_locked)
    );

    logic rst, rst_raw;

    asc_reset_filter u_rst (
        .clk         (clk),
        .rst_n_async (pi_rst_n),
        .rst         (rst),
        .rst_raw     (rst_raw)
    );

    /* verilator lint_off UNUSEDSIGNAL */
    logic [199:0] src_a;   // [199:197] reserved
    /* verilator lint_on UNUSEDSIGNAL */
    logic [463:0] probe_a;
    logic [495:0] probe_b;
    /* verilator lint_off UNUSEDSIGNAL */
    logic         src_b;
    /* verilator lint_on UNUSEDSIGNAL */
    logic [25:0]  src_c;
    logic [479:0] probe_c;
    // asc_bus link faults by kind and LINK_ERROR results (wrapping, never
    // cleared; counted next to asc_bus).
    /* verilator lint_off PROCASSINIT */
    logic [31:0]  flt_to_q = '0;
    logic [31:0]  flt_bad_q = '0;
    logic [31:0]  flt_state_q = '0;
    logic [31:0]  flt_link_q = '0;
    logic [31:0]  tear_n_q = '0;      // tears applied (tear test)
    /* verilator lint_on PROCASSINIT */

    // Chip clock divider: high for div/2 of every div fast cycles, as the
    // Pico's PWM. The divider setting crosses from the T4C source clock
    // through two flops; the host changes it only with the chip in reset.
    logic [7:0] ck_div_m_q, ck_div_q, ck_cnt_q;

    always_ff @(posedge fclk) begin
        ck_div_m_q <= src_c[24:17];
        ck_div_q   <= ck_div_m_q;
        ck_sel_m_q <= src_c[25];
        ck_sel_q   <= ck_sel_m_q;
        if (ck_cnt_q + 8'd1 >= ck_div_q) begin
            ck_cnt_q <= '0;
        end else begin
            ck_cnt_q <= ck_cnt_q + 8'd1;
        end
        ck_q <= (ck_cnt_q < {1'b0, ck_div_q[7:1]});
    end

    logic        man_go_t, stress_run, stop_on_err, clr_t;
    logic [11:0] op_mask;
    logic [3:0]  dly_in, dly_out;
    logic [7:0]  man_hdr;
    logic [23:0] man_addr;
    logic [31:0] man_wdata;
    logic [31:0] seed, fseed;
    logic [15:0] ram_words;
    logic [17:0] flash_words;
    logic [3:0]  run_log2;

    always_comb begin
        man_go_t    = src_a[0];
        stress_run  = src_a[1];
        stop_on_err = src_a[2];
        clr_t       = src_a[3];
        op_mask     = src_a[15:4];
        dly_in      = src_a[19:16];
        dly_out     = src_a[23:20];
        man_hdr     = src_a[31:24];
        man_addr    = src_a[55:32];
        man_wdata   = src_a[87:56];
        seed        = src_a[119:88];
        fseed       = src_a[151:120];
        ram_words   = src_a[167:152];
        flash_words = src_a[185:168];
        run_log2    = src_a[189:186];
    end

    /* Pins: delays, bus master, safety net */

    logic [7:0] uio_d;
    logic       ack_d, irq_d;
    logic       bus_oe, bus_req, bus_frame;
    logic [7:0] bus_out;
    logic       oe_d, req_d, frame_d;
    logic [7:0] out_d;
    logic       pdrv_meta_q, pdrv_q;

    dly_line #(.W(10)) u_dly_in (
        .clk (dclk),
        .sel (dly_in),
        .d   ({pi_irq, pi_ack, uio}),
        .q   ({irq_d, ack_d, uio_d})
    );

    dly_line #(.W(11)) u_dly_out (
        .clk (dclk),
        .sel (dly_out),
        .d   ({bus_frame, bus_oe, bus_req, bus_out}),
        .q   ({frame_d, oe_d, req_d, out_d})
    );

    always_ff @(posedge dclk) begin
        pdrv_meta_q <= pi_dbg_di;
        pdrv_q      <= pdrv_meta_q;
    end

    // Safety net: never drive while the Pico says it does. The violation
    // count is in fast clock cycles and is never cleared.
    /* verilator lint_off PROCASSINIT */
    logic [15:0] viol_q = '0;
    /* verilator lint_on PROCASSINIT */

    always_ff @(posedge dclk) begin
        if (oe_d && pdrv_q && viol_q != 16'hFFFF) begin
            viol_q <= viol_q + 16'd1;
        end
    end

    // frame at the pins, after the output delays, as a bridge sees it
    // (T4C). Every REQ change and every cycle of uio_oe must fall inside
    // frame. frame rises one chip clock before the header's REQ change
    // and falls only after the RP's last answer, so a REQ change never
    // comes close to a frame edge. frame and uio_oe can change on the
    // same chip clock edge, so one fast cycle of difference is skew and
    // only two in a row count. A reset clears REQ with frame low: changes
    // while the chip is in reset are not counted.
    /* verilator lint_off PROCASSINIT */
    logic [1:0]  fr_s_q = '0;
    logic [1:0]  foe_s_q = '0;
    logic [2:0]  freq_s_q = '0;
    logic [1:0]  frst_s_q = '0;
    logic        foe_bad_prev_q = 1'b0;
    logic [15:0] fr_req_bad_q = '0;
    logic [15:0] fr_oe_bad_q = '0;
    /* verilator lint_on PROCASSINIT */
    logic        foe_bad;

    always_comb begin
        foe_bad = foe_s_q[1] && !fr_s_q[1];
    end

    always_ff @(posedge dclk) begin
        fr_s_q         <= {fr_s_q[0], frame_d};
        foe_s_q        <= {foe_s_q[0], oe_d};
        freq_s_q       <= {freq_s_q[1:0], req_d};
        frst_s_q       <= {frst_s_q[0], rst};
        foe_bad_prev_q <= foe_bad;
        if (freq_s_q[2] != freq_s_q[1] && !fr_s_q[1] && !frst_s_q[1]
            && fr_req_bad_q != 16'hFFFF) begin
            fr_req_bad_q <= fr_req_bad_q + 16'd1;
        end
        if (foe_bad && foe_bad_prev_q && fr_oe_bad_q != 16'hFFFF) begin
            fr_oe_bad_q <= fr_oe_bad_q + 16'd1;
        end
    end

    // Overlap events and long overlap events (see the T4B probes).
    /* verilator lint_off PROCASSINIT */
    logic        ovl_prev_q = 1'b0;
    logic [1:0]  ovl_run_q = '0;
    logic [15:0] ovl_ev_q = '0;
    logic [31:0] ovl_long_q = '0;
    /* verilator lint_on PROCASSINIT */
    logic        ovl;

    always_comb begin
        ovl = oe_d && pdrv_q;
    end

    always_ff @(posedge dclk) begin
        ovl_prev_q <= ovl;
        if (!ovl) begin
            ovl_run_q <= '0;
        end else if (ovl_run_q != 2'd3) begin
            ovl_run_q <= ovl_run_q + 2'd1;
        end
        if (ovl && !ovl_prev_q) begin
            ovl_ev_q <= ovl_ev_q + 16'd1;
        end
        if (ovl && ovl_run_q == 2'd2) begin
            ovl_long_q <= ovl_long_q + 32'd1;
        end
    end

    // CLK wire integrity monitor (see the T4C probes). Bench only.
    /* verilator lint_off PROCASSINIT */
    logic        clk_tog_q = 1'b0;
    /* verilator lint_on PROCASSINIT */

    always_ff @(posedge pi_clk) begin
        clk_tog_q <= ~clk_tog_q;
    end

    logic [2:0]  tog_s_q;
    logic [2:0]  pin_s_q;
    logic [1:0]  req_s_q;
    logic        cm_clr_q;
    logic        per_ok_q;
    logic        cm_run_ok_q;
    logic [7:0]  per_q;
    logic [7:0]  cm_run_q;
    logic [31:0] per_n_q;
    logic [31:0] per_short_q;
    logic [31:0] per_long_q;
    logic [7:0]  per_min_q;
    logic [7:0]  per_max_q;
    logic [7:0]  hi_min_q;
    logic [7:0]  lo_min_q;
    logic [31:0] t_now_q;
    logic [31:0] t_anom_q;
    logic [31:0] t_req_q;
    logic        cm_clr_t;
    logic [7:0]  cm_lo, cm_hi;

    always_comb begin
        cm_clr_t = src_c[0];
        cm_lo    = src_c[8:1];
        cm_hi    = src_c[16:9];
    end

    always_ff @(posedge dclk) begin
        tog_s_q  <= {tog_s_q[1:0], clk_tog_q};
        pin_s_q  <= {pin_s_q[1:0], pi_clk};
        req_s_q  <= {req_s_q[0], req_d};
        cm_clr_q <= cm_clr_t;
        t_now_q  <= t_now_q + 32'd1;
        if (req_s_q[1] != req_s_q[0]) begin
            t_req_q <= t_now_q;
        end
        if (tog_s_q[2] != tog_s_q[1]) begin
            per_q    <= 8'd1;
            per_ok_q <= 1'b1;
            if (per_ok_q) begin
                per_n_q <= per_n_q + 32'd1;
                if (per_q < per_min_q) begin
                    per_min_q <= per_q;
                end
                if (per_q > per_max_q) begin
                    per_max_q <= per_q;
                end
                if (per_q < cm_lo) begin
                    per_short_q <= per_short_q + 32'd1;
                    t_anom_q    <= t_now_q;
                end
                if (per_q > cm_hi) begin
                    per_long_q <= per_long_q + 32'd1;
                    t_anom_q   <= t_now_q;
                end
            end
        end else if (per_q != 8'hFF) begin
            per_q <= per_q + 8'd1;
        end
        if (pin_s_q[2] != pin_s_q[1]) begin
            cm_run_q    <= 8'd1;
            cm_run_ok_q <= 1'b1;
            if (cm_run_ok_q) begin
                if (pin_s_q[2] && cm_run_q < hi_min_q) begin
                    hi_min_q <= cm_run_q;
                end
                if (!pin_s_q[2] && cm_run_q < lo_min_q) begin
                    lo_min_q <= cm_run_q;
                end
            end
        end else if (cm_run_q != 8'hFF) begin
            cm_run_q <= cm_run_q + 8'd1;
        end
        if (cm_clr_q != cm_clr_t) begin
            per_ok_q    <= 1'b0;
            cm_run_ok_q    <= 1'b0;
            per_n_q     <= '0;
            per_short_q <= '0;
            per_long_q  <= '0;
            per_min_q   <= 8'hFF;
            per_max_q   <= '0;
            hi_min_q    <= 8'hFF;
            lo_min_q    <= 8'hFF;
            t_anom_q    <= '0;
        end
    end

    always_comb begin
        probe_c = {tear_n_q, fr_oe_bad_q, fr_req_bad_q, fr_bal_q, fr_rise_q,
                   flt_link_q, flt_state_q, flt_bad_q, flt_to_q,
                   t_now_q, t_req_q, t_anom_q, lo_min_q, hi_min_q, per_max_q, per_min_q,
                   per_long_q, per_short_q, per_n_q};
    end

    // A tri-state net cannot be driven from always_comb.
    assign uio = (oe_d && !pdrv_q) ? out_d : 8'bzzzz_zzzz;

    always_comb begin
        req    = req_d;
        dbg_do = oe_d;
    end

    logic        bus_start, bus_idle, bus_done;
    logic [7:0]  bus_hdr, bus_status;
    logic [23:0] bus_addr;
    logic [31:0] bus_wdata, bus_rdata;
    logic        ack_glitch;
    logic        bus_fault;
    logic [1:0]  bus_fault_why;
    logic [15:0] bus_dbg;
    logic        tear;

    asc_bus #(
        .TO_BITS (BUS_TO_BITS)
    ) u_bus (
        .clk        (clk),
        .rst        (rst),
        .start      (bus_start),
        .idle       (bus_idle),
        .hdr        (bus_hdr),
        .addr       ({8'd0, bus_addr}),
        .wdata      (bus_wdata),
        .done       (bus_done),
        .status     (bus_status),
        .rdata      (bus_rdata),
        .uio_in     (uio_d),
        .uio_out    (bus_out),
        .uio_oe     (bus_oe),
        .req        (bus_req),
        .frame      (bus_frame),
        .ack_async  (ack_d),
        .test_tear  (tear),
        .ack_glitch (ack_glitch),
        .fault      (bus_fault),
        .fault_why  (bus_fault_why),
        .dbg        (bus_dbg)
    );

    // Link faults by kind and LINK_ERROR results (T4C probes).
    always_ff @(posedge clk) begin
        if (bus_fault && bus_fault_why == 2'd1) begin
            flt_to_q <= flt_to_q + 32'd1;
        end
        if (bus_fault && bus_fault_why == 2'd2) begin
            flt_bad_q <= flt_bad_q + 32'd1;
        end
        if (bus_fault && bus_fault_why == 2'd3) begin
            flt_state_q <= flt_state_q + 32'd1;
        end
        if (bus_done && bus_status == 8'd5) begin
            flt_link_q <= flt_link_q + 32'd1;
        end
    end

    // Tear test (see the header). The transaction count steps when the
    // bus unit takes a start (start up, idle gone low).
    logic        tear_on;
    logic [1:0]  tear_sel;
    logic [3:0]  tear_log2;
    logic [15:0] tear_mask;
    logic [2:0]  tear_state;
    logic        tear_arm_q;
    logic [7:0]  tear_wait_q;
    logic [15:0] tear_k_q;
    /* verilator lint_off PROCASSINIT */
    logic [15:0] tear_rnd_q = 16'hACE1;
    /* verilator lint_on PROCASSINIT */

    always_comb begin
        tear_on   = src_a[190];
        tear_sel  = src_a[192:191];
        tear_log2 = src_a[196:193];
        tear_mask = (16'd1 << tear_log2) - 16'd1;
        case (tear_sel)
            2'd0:    tear_state = 3'd1;   // SEND_TOGGLE
            2'd1:    tear_state = 3'd2;   // SEND_WAIT
            2'd2:    tear_state = 3'd3;   // HANDOFF
            default: tear_state = 3'd5;   // RECV_TOGGLE
        endcase
        tear = tear_on && tear_arm_q && (tear_wait_q == 8'd0) && (bus_dbg[15:13] == tear_state);
    end

    always_ff @(posedge clk) begin
        tear_rnd_q <= {tear_rnd_q[14:0], tear_rnd_q[15] ^ tear_rnd_q[13] ^ tear_rnd_q[12] ^ tear_rnd_q[10]};
        if (rst || !tear_on) begin
            tear_arm_q  <= 1'b0;
            tear_wait_q <= '0;
            tear_k_q    <= '0;
        end else begin
            if (tear) begin
                tear_arm_q <= 1'b0;
                tear_n_q   <= tear_n_q + 32'd1;
            end else if (tear_arm_q && tear_wait_q != 8'd0) begin
                tear_wait_q <= tear_wait_q - 8'd1;
            end
            if (bus_start && !bus_idle) begin
                tear_k_q <= tear_k_q + 16'd1;
                if (!tear_arm_q && (tear_k_q & tear_mask) == 16'd0) begin
                    tear_arm_q  <= 1'b1;
                    tear_wait_q <= tear_rnd_q[7:0];
                end
            end
        end
    end

    // IRQ: the chip's synchronizer and three-sample filter.
    logic irq_q;

    asc_irq_filter u_irq (
        .clk       (clk),
        .irq_async (irq_d),
        .irq       (irq_q)
    );

    // frame balance (T4C): every rise of frame is a start or a retry, and
    // a retry is a link fault that did not end in LINK_ERROR.
    /* verilator lint_off PROCASSINIT */
    logic        fr_prev_q = 1'b0;
    logic [31:0] fr_rise_q = '0;
    logic [31:0] fr_bal_q = '0;
    /* verilator lint_on PROCASSINIT */
    logic        fr_rise, fr_start, fr_link;

    always_comb begin
        fr_rise  = bus_frame && !fr_prev_q;
        fr_start = bus_start && bus_idle && !rst;
        fr_link  = bus_done && bus_status == 8'd5;
    end

    always_ff @(posedge clk) begin
        fr_prev_q <= bus_frame;
        fr_rise_q <= fr_rise_q + {31'd0, fr_rise};
        fr_bal_q  <= fr_bal_q + {31'd0, fr_rise} - {31'd0, fr_start} - {31'd0, bus_fault}
                     + {31'd0, fr_link};
    end

    // Diagnostics that only configuration clears.
    /* verilator lint_off PROCASSINIT */
    logic        rst_prev_q = 1'b0;
    logic        rst_raw_prev_q = 1'b0;
    logic [15:0] rst_count_q = '0;
    logic [31:0] rst_short_q = '0;
    logic [31:0] ack_glitch_q = '0;
    /* verilator lint_on PROCASSINIT */

    always_ff @(posedge clk) begin
        rst_prev_q     <= rst;
        rst_raw_prev_q <= rst_raw;
        if (rst && !rst_prev_q && rst_count_q != 16'hFFFF) begin
            rst_count_q <= rst_count_q + 16'd1;
        end
        if (!rst_raw && rst_raw_prev_q && !rst) begin
            rst_short_q <= rst_short_q + 32'd1;
        end
        if (ack_glitch) begin
            ack_glitch_q <= ack_glitch_q + 32'd1;
        end
    end

    /* Test data */

    // A xorshift of address and seed: RAM words hold h(address, seed),
    // flash words h(offset, fseed), disk words h({gen, index, 00}, seed ^
    // 0xD15C0000). bench/firmware computes the same function for flash.
    function automatic logic [31:0] h(input logic [23:0] a, input logic [31:0] s);
        logic [31:0] x;
        x = {8'h00, a} ^ s;
        x = x ^ (x << 13);
        x = x ^ (x >> 17);
        x = x ^ (x << 5);
        return x;
    endfunction

    function automatic logic [31:0] lane_mask(input logic [1:0] sz);
        case (sz)
            2'd0:    return 32'h000000FF;
            2'd1:    return 32'h0000FFFF;
            default: return 32'hFFFFFFFF;
        endcase
    endfunction

    /* Sequencer */

    typedef enum logic [3:0] {
        OP_RAM_RD, OP_RAM_WR, OP_RAM_FETCH, OP_RAM_NEXT, OP_FLASH_RD,
        OP_FLASH_FETCH, OP_TIMER, OP_SCRATCH, OP_CONSOLE, OP_DISK, OP_FAULT,
        OP_FILL
    } op_t;

    typedef enum logic [3:0] {
        D_IDLE, D_FILL, D_SEC, D_BUFW, D_CMDW, D_POLLW, D_BUFR, D_CMDR,
        D_POLLR, D_CHECK
    } disk_t;

    typedef enum logic [2:0] { T_IDLE, T_PLAN, T_ISSUE, T_WAIT, T_EVAL } tstate_t;
    typedef enum logic [1:0] { C_ST, C_DATA, C_TIMER, C_CAP } chk_t;

    localparam logic [3:0] E_RAM = 4'd1, E_FLASH = 4'd2, E_TIMER = 4'd3,
                           E_SCRATCH = 4'd4, E_CONSOLE = 4'd5, E_DISK_DATA = 4'd6,
                           E_DISK = 4'd7, E_FAULT = 4'd8, E_IRQ = 4'd10;

    tstate_t     t_q;
    op_t         op_q;
    disk_t       disk_q;
    logic        manual_q;
    logic [1:0]  step_q;
    logic [15:0] cnt_q;
    logic [2:0]  sub_q;
    logic [23:0] run_q;
    logic [31:0] sv_q;
    logic [7:0]  con_q;
    logic [5:0]  sector_q;
    logic [11:0] gen_q;
    logic [9:0]  dcnt_q;
    logic [31:0] rnd_q;
    logic [31:0] t_prev_q;
    logic        t_valid_q;
    logic        irq_armed_q;
    logic        irq_tx_q;

    logic [7:0]  hdr_q;
    logic [23:0] addr_q;
    logic [31:0] wdata_q;
    logic [7:0]  exp_st_q;
    logic [31:0] exp_q;
    chk_t        chk_q;
    logic [3:0]  ekind_q;

    logic        man_go_seen_q, clr_seen_q, man_done_t_q;
    logic [7:0]  man_status_q;
    logic [31:0] man_rdata_q;
    logic [31:0] tx_q, err_q;
    logic [39:0] rt_q;
    logic [47:0] cyc_q;
    logic [31:0] txcyc_q, txmax_q;
    logic        e_valid_q;
    logic [3:0]  e_kind_q;
    logic [7:0]  e_status_q, e_hdr_q;
    logic [23:0] e_addr_q;
    logic [31:0] e_exp_q, e_got_q;
    logic        req_prev_q;
    logic [31:0] cnt_op_q [14];

    // Derived values.
    logic [15:0] run_len;
    logic [11:0] mask_eff;
    logic [15:0] ram_idx, ram_run_idx;
    logic [17:0] fl_idx, fl_run_idx;
    logic [15:0] ram_span;
    logic [17:0] fl_span;
    /* verilator lint_off UNUSEDSIGNAL */
    logic [31:0] mul_a, mul_b;   // only the high halves are used
    logic [35:0] mul_c, mul_d;
    /* verilator lint_on UNUSEDSIGNAL */
    logic [3:0]  pick;
    logic        atomic;
    logic        stress_active;
    logic        st_ok;
    logic        bad;
    logic [31:0] rnd_next;
    logic [3:0]  pick_start;
    logic [4:0]  pick_c;
    logic [1:0]  m_sz;
    logic [1:0]  m_off;
    logic [23:0] m_wa;
    logic [31:0] m_v;
    logic [23:0] r_base;
    logic [31:0] r_seed;
    logic [3:0]  r_ek;
    logic [7:0]  r_hf, r_hn;
    logic [23:0] r_ra;
    logic [2:0]  f_sb;
    logic        ev_done;
    logic        ev_err;

    always_comb begin
        run_len = 16'd1 << run_log2;

        mask_eff = op_mask;
        if (ram_words == 16'd0) begin
            mask_eff[0] = 1'b0;
            mask_eff[1] = 1'b0;
        end
        if (ram_words <= run_len) begin
            mask_eff[2] = 1'b0;
            mask_eff[3] = 1'b0;
        end
        if (flash_words == 18'd0) begin
            mask_eff[4] = 1'b0;
        end
        if (flash_words <= {2'b00, run_len}) begin
            mask_eff[5] = 1'b0;
        end
        mask_eff[11] = 1'b0;  // IRQ is answered, not picked

        // Scaled random indices: (r * n) >> width, uniform over [0, n).
        ram_span = ram_words - run_len;
        fl_span  = flash_words - {2'b00, run_len};
        mul_a = {16'h0, rnd_q[31:16]} * {16'h0, ram_words};
        mul_b = {16'h0, rnd_q[31:16]} * {16'h0, ram_span};
        mul_c = {18'h0, rnd_q[31:14]} * {18'h0, flash_words};
        mul_d = {18'h0, rnd_q[31:14]} * {18'h0, fl_span};
        ram_idx     = mul_a[31:16];
        ram_run_idx = mul_b[31:16];
        fl_idx      = mul_c[35:18];
        fl_run_idx  = mul_d[35:18];

        // Next op: the first enabled one from a random start. Starts 11 to
        // 15 fall on the memory ops, which therefore come up more often.
        pick = 4'hF;
        case (rnd_q[3:0])
            4'd11:   pick_start = 4'd0;
            4'd12:   pick_start = 4'd2;
            4'd13:   pick_start = 4'd3;
            4'd14:   pick_start = 4'd4;
            4'd15:   pick_start = 4'd5;
            default: pick_start = rnd_q[3:0];
        endcase
        pick_c = 5'd0;
        for (int k = 10; k >= 0; k--) begin
            pick_c = {1'b0, pick_start} + 5'(k);
            if (pick_c > 5'd10) begin
                pick_c = pick_c - 5'd11;
            end
            if (mask_eff[pick_c[3:0]]) begin
                pick = pick_c[3:0];
            end
        end

        // RAM read and write: size, lane and the bytes that belong there.
        m_sz  = (rnd_q[1:0] == 2'd3) ? 2'd2 : rnd_q[1:0];
        m_off = (m_sz == 2'd0) ? rnd_q[3:2] : (m_sz == 2'd1) ? {rnd_q[3], 1'b0} : 2'b00;
        m_wa  = {6'b0, ram_idx, 2'b00};
        m_v   = (h(m_wa, seed) >> {m_off, 3'b000}) & lane_mask(m_sz);

        // Runs: RAM or flash, fetch or data.
        if (op_q == OP_FLASH_FETCH) begin
            r_base = FLASH_BASE;
            r_seed = fseed;
            r_ek   = E_FLASH;
            r_ra   = {4'b0, fl_run_idx, 2'b00};
        end else begin
            r_base = 24'h000000;
            r_seed = seed;
            r_ek   = E_RAM;
            r_ra   = {6'b0, ram_run_idx, 2'b00};
        end
        if (op_q == OP_RAM_NEXT) begin
            r_hf = 8'h24;
            r_hn = 8'h64;
        end else begin
            r_hf = 8'h2C;
            r_hn = 8'h6C;
        end

        f_sb = (step_q == 2'd0) ? rnd_q[2:0] : sub_q;

        atomic = (op_q == OP_RAM_NEXT && step_q != 2'd0) ||
                 (op_q == OP_FAULT && sub_q == 3'd5 && step_q != 2'd0);
        stress_active = (t_q != T_IDLE) && !manual_q;

        st_ok = (bus_status == exp_st_q);
        case (chk_q)
            C_DATA:  bad = !st_ok || (bus_rdata != exp_q);
            C_TIMER: bad = !st_ok || (t_valid_q && (bus_rdata - t_prev_q) >= 32'h80000000);
            default: bad = !st_ok;
        endcase

        rnd_next = rnd_q ^ (rnd_q << 13);
        rnd_next = rnd_next ^ (rnd_next >> 17);
        rnd_next = rnd_next ^ (rnd_next << 5);

        // Evaluation of the finished transaction: is the op done, and is
        // this an error.
        ev_done = 1'b1;
        ev_err  = bad;
        if (!irq_tx_q) begin
            case (op_q)
                OP_FILL: begin
                    ev_done = (cnt_q + 16'd1 >= ram_words);
                end
                OP_RAM_FETCH, OP_RAM_NEXT, OP_FLASH_FETCH: begin
                    ev_done = (cnt_q == 16'd0);
                end
                OP_SCRATCH: begin
                    ev_done = (step_q != 2'd0);
                end
                OP_FAULT: begin
                    ev_done = !(sub_q == 3'd5 && step_q == 2'd0);
                end
                OP_CONSOLE: begin
                    if (step_q == 2'd0) begin
                        ev_done = !(st_ok && bus_rdata[0]);
                    end else if (step_q == 2'd1) begin
                        ev_done = !st_ok || bus_rdata[8];
                        // The status said a byte was waiting.
                        ev_err  = bad || bus_rdata[8];
                    end
                end
                OP_DISK: begin
                    // A flash error reported by the RP.
                    if ((disk_q == D_POLLW || disk_q == D_POLLR) && st_ok && !bus_rdata[0] && bus_rdata[1]) begin
                        ev_err = 1'b1;
                    end
                end
                default: begin
                end
            endcase
        end else begin
            ev_done = 1'b0;
        end

        bus_start = (t_q == T_ISSUE);
        bus_hdr   = hdr_q;
        bus_addr  = addr_q;
        bus_wdata = wdata_q;
    end

    // One transaction with the expected answer.
    task automatic plan(input logic [7:0] h_, input logic [23:0] a_, input logic [31:0] w_,
                        input logic [7:0] est, input logic [31:0] ex, input chk_t ck,
                        input logic [3:0] ek);
        hdr_q    <= h_;
        addr_q   <= a_;
        wdata_q  <= w_;
        exp_st_q <= est;
        exp_q    <= ex;
        chk_q    <= ck;
        ekind_q  <= ek;
        t_q      <= T_ISSUE;
    endtask

    always_ff @(posedge clk) begin
        req_prev_q <= bus_req;
        if (!irq_q) begin
            irq_armed_q <= 1'b1;
        end
        if (rst) begin
            t_q           <= T_IDLE;
            op_q          <= OP_FILL;
            disk_q        <= D_IDLE;
            manual_q      <= 1'b0;
            step_q        <= '0;
            cnt_q         <= '0;
            sub_q         <= '0;
            run_q         <= '0;
            sv_q          <= '0;
            con_q         <= '0;
            sector_q      <= '0;
            gen_q         <= '0;
            dcnt_q        <= '0;
            rnd_q         <= 32'h2545F491;
            t_prev_q      <= '0;
            t_valid_q     <= 1'b0;
            irq_armed_q   <= 1'b0;
            irq_tx_q      <= 1'b0;
            hdr_q         <= '0;
            addr_q        <= '0;
            wdata_q       <= '0;
            exp_st_q      <= '0;
            exp_q         <= '0;
            chk_q         <= C_ST;
            ekind_q       <= '0;
            man_go_seen_q <= man_go_t;
            clr_seen_q    <= clr_t;
            man_done_t_q  <= 1'b0;
            man_status_q  <= '0;
            man_rdata_q   <= '0;
            tx_q          <= '0;
            err_q         <= '0;
            rt_q          <= '0;
            cyc_q         <= '0;
            txcyc_q       <= '0;
            txmax_q       <= '0;
            e_valid_q     <= 1'b0;
            e_kind_q      <= '0;
            e_status_q    <= '0;
            e_hdr_q       <= '0;
            e_addr_q      <= '0;
            e_exp_q       <= '0;
            e_got_q       <= '0;
            for (int k = 0; k < 14; k++) begin
                cnt_op_q[k] <= '0;
            end
        end else begin
            if (stress_active) begin
                cyc_q <= cyc_q + 48'd1;
                if (bus_req != req_prev_q) begin
                    rt_q <= rt_q + 40'd1;
                end
            end
            if (t_q == T_WAIT) begin
                txcyc_q <= txcyc_q + 32'd1;
            end
            if (clr_t != clr_seen_q) begin
                clr_seen_q <= clr_t;
                tx_q       <= '0;
                err_q      <= '0;
                rt_q       <= '0;
                cyc_q      <= '0;
                txmax_q    <= '0;
                e_valid_q  <= 1'b0;
                for (int k = 0; k < 14; k++) begin
                    cnt_op_q[k] <= '0;
                end
            end

            case (t_q)
                T_IDLE: begin
                    if (man_go_t != man_go_seen_q) begin
                        man_go_seen_q <= man_go_t;
                        manual_q      <= 1'b1;
                        hdr_q         <= man_hdr;
                        addr_q        <= man_addr;
                        wdata_q       <= man_wdata;
                        t_q           <= T_ISSUE;
                    end else if (stress_run && mask_eff != 12'h000) begin
                        manual_q  <= 1'b0;
                        rnd_q     <= seed ^ 32'h2545F491;
                        t_valid_q <= 1'b0;
                        irq_tx_q  <= 1'b0;
                        disk_q    <= D_IDLE;
                        step_q    <= '0;
                        cnt_q     <= '0;
                        if (mask_eff[1:0] != 2'b00 || mask_eff[3:2] != 2'b00) begin
                            op_q <= OP_FILL;
                        end else begin
                            op_q <= op_t'(pick);
                        end
                        t_q <= T_PLAN;
                    end
                end

                T_PLAN: begin
                    irq_tx_q <= 1'b0;
                    if (op_mask[11] && irq_q && irq_armed_q && !atomic) begin
                        irq_tx_q    <= 1'b1;
                        irq_armed_q <= 1'b0;
                        plan(8'hA4, R_IRQ_ACK, 32'h1, ST_OK, '0, C_ST, E_IRQ);
                    end else begin
                        case (op_q)
                            OP_FILL: begin
                                plan(8'hA4, {6'b0, cnt_q, 2'b00}, h({6'b0, cnt_q, 2'b00}, seed),
                                     ST_OK, '0, C_ST, E_RAM);
                            end
                            OP_RAM_RD: begin
                                plan({2'b00, m_sz, 1'b0, rnd_q[4], 2'b00}, m_wa | {22'b0, m_off}, '0,
                                     ST_OK, m_v, C_DATA, E_RAM);
                            end
                            OP_RAM_WR: begin
                                plan({2'b10, m_sz, 1'b0, rnd_q[4], 2'b00}, m_wa | {22'b0, m_off}, m_v,
                                     ST_OK, '0, C_ST, E_RAM);
                            end
                            OP_RAM_FETCH, OP_RAM_NEXT, OP_FLASH_FETCH: begin
                                if (step_q == 2'd0) begin
                                    run_q <= r_ra;
                                    cnt_q <= run_len - 16'd1;
                                    plan(r_hf, r_base + r_ra, '0, ST_OK, h(r_ra, r_seed), C_DATA, r_ek);
                                end else begin
                                    plan(r_hn, 24'h000000, '0, ST_OK, h(run_q, r_seed), C_DATA, r_ek);
                                end
                            end
                            OP_FLASH_RD: begin
                                plan(8'h24, FLASH_BASE + {4'b0, fl_idx, 2'b00}, '0, ST_OK,
                                     h({4'b0, fl_idx, 2'b00}, fseed), C_DATA, E_FLASH);
                            end
                            OP_TIMER: begin
                                plan(8'h24, R_TIMER, '0, ST_OK, '0, C_TIMER, E_TIMER);
                            end
                            OP_SCRATCH: begin
                                if (step_q == 2'd0) begin
                                    sv_q <= rnd_q;
                                    plan(8'hA4, R_SCRATCH, rnd_q, ST_OK, '0, C_ST, E_SCRATCH);
                                end else begin
                                    plan(8'h24, R_SCRATCH, '0, ST_OK, sv_q, C_DATA, E_SCRATCH);
                                end
                            end
                            OP_CONSOLE: begin
                                case (step_q)
                                    2'd0:    plan(8'h24, R_CON_STAT, '0, ST_OK, '0, C_CAP, E_CONSOLE);
                                    2'd1:    plan(8'h24, R_CON_DATA, '0, ST_OK, '0, C_CAP, E_CONSOLE);
                                    default: plan(8'hA4, R_CON_DATA, {24'h0, con_q}, ST_OK, '0, C_ST, E_CONSOLE);
                                endcase
                            end
                            OP_DISK: begin
                                case (disk_q)
                                    D_IDLE: begin
                                        // Choose the sector and data, then plan again.
                                        sector_q <= rnd_q[5:0];
                                        gen_q    <= gen_q + 12'd1;
                                        dcnt_q   <= '0;
                                        disk_q   <= D_FILL;
                                    end
                                    D_FILL: begin
                                        plan(8'hA4, BUF_A + {12'b0, dcnt_q, 2'b00},
                                             h({gen_q, dcnt_q, 2'b00}, seed ^ 32'hD15C0000),
                                             ST_OK, '0, C_ST, E_DISK);
                                    end
                                    D_SEC:   plan(8'hA4, R_DISK_SEC, {26'b0, sector_q}, ST_OK, '0, C_ST, E_DISK);
                                    D_BUFW:  plan(8'hA4, R_DISK_BUF, {8'b0, BUF_A}, ST_OK, '0, C_ST, E_DISK);
                                    D_CMDW:  plan(8'hA4, R_DISK_CMD, 32'h2, ST_OK, '0, C_ST, E_DISK);
                                    D_BUFR:  plan(8'hA4, R_DISK_BUF, {8'b0, BUF_B}, ST_OK, '0, C_ST, E_DISK);
                                    D_CMDR:  plan(8'hA4, R_DISK_CMD, 32'h1, ST_OK, '0, C_ST, E_DISK);
                                    D_POLLW, D_POLLR: begin
                                        plan(8'h24, R_DISK_STAT, '0, ST_OK, '0, C_CAP, E_DISK);
                                    end
                                    default: begin
                                        plan(8'h24, BUF_B + {12'b0, dcnt_q, 2'b00}, '0, ST_OK,
                                             h({gen_q, dcnt_q, 2'b00}, seed ^ 32'hD15C0000),
                                             C_DATA, E_DISK_DATA);
                                    end
                                endcase
                            end
                            default: begin  // OP_FAULT
                                sub_q <= f_sb;
                                case (f_sb)
                                    3'd0: plan(8'h24, 24'h000002, '0, ST_MISALIGNED, '0, C_ST, E_FAULT);
                                    3'd1: plan(8'h24, 24'h400000, '0, ST_BUS_ERROR, '0, C_ST, E_FAULT);
                                    3'd2: plan(8'h20, KPAGE, '0, ST_PROTECTION, '0, C_ST, E_FAULT);
                                    3'd3: plan(8'h2C, DEV_BASE, '0, ST_PROTECTION, '0, C_ST, E_FAULT);
                                    3'd4: plan(8'hA4, FLASH_BASE, '0, ST_BUS_ERROR, '0, C_ST, E_FAULT);
                                    3'd5: begin
                                        if (step_q == 2'd0) begin
                                            plan(8'h24, 24'h000102, '0, ST_MISALIGNED, '0, C_ST, E_FAULT);
                                        end else begin
                                            plan(8'h64, 24'h000000, '0, ST_BAD_NEXT, '0, C_ST, E_FAULT);
                                        end
                                    end
                                    3'd6: plan(8'h34, 24'h000100, '0, ST_BUS_ERROR, '0, C_ST, E_FAULT);
                                    default: plan(8'h04, R_SCRATCH, '0, ST_BUS_ERROR, '0, C_ST, E_FAULT);
                                endcase
                            end
                        endcase
                    end
                end

                // The core's rule (asc_bus.sv): start, hdr, addr
                // and wdata stay up until idle goes low (the bus unit took
                // them), then the end is idle high again. The one-cycle
                // done pulse is only counted, never waited for: a bad
                // clock edge can make this side miss it.
                T_ISSUE: begin
                    if (!bus_idle) begin
                        txcyc_q <= '0;
                        t_q     <= T_WAIT;
                    end
                end

                T_WAIT: begin
                    if (bus_idle) begin
                        if (manual_q) begin
                            man_status_q <= bus_status;
                            man_rdata_q  <= bus_rdata;
                            man_done_t_q <= ~man_done_t_q;
                            manual_q     <= 1'b0;
                            t_q          <= T_IDLE;
                        end else begin
                            t_q <= T_EVAL;
                        end
                    end
                end

                T_EVAL: begin
                    tx_q    <= tx_q + 32'd1;
                    rnd_q   <= rnd_next;
                    if (txcyc_q > txmax_q) begin
                        txmax_q <= txcyc_q;
                    end
                    if (irq_tx_q) begin
                        cnt_op_q[12] <= cnt_op_q[12] + 32'd1;
                    end else begin
                        cnt_op_q[op_q] <= cnt_op_q[op_q] + 32'd1;
                        if (chk_q == C_TIMER && st_ok) begin
                            t_prev_q  <= bus_rdata;
                            t_valid_q <= 1'b1;
                        end
                        case (op_q)
                            OP_FILL: begin
                                if (!ev_done) begin
                                    cnt_q <= cnt_q + 16'd1;
                                end
                            end
                            OP_RAM_FETCH, OP_RAM_NEXT, OP_FLASH_FETCH: begin
                                run_q <= run_q + 24'd4;
                                if (!ev_done) begin
                                    cnt_q  <= cnt_q - 16'd1;
                                    step_q <= 2'd1;
                                end
                            end
                            OP_SCRATCH, OP_FAULT: begin
                                if (!ev_done) begin
                                    step_q <= 2'd1;
                                end
                            end
                            OP_CONSOLE: begin
                                if (!ev_done) begin
                                    step_q <= step_q + 2'd1;
                                    if (step_q == 2'd1) begin
                                        con_q <= bus_rdata[7:0];
                                    end
                                end
                            end
                            OP_DISK: begin
                                case (disk_q)
                                    D_FILL: begin
                                        dcnt_q <= dcnt_q + 10'd1;
                                        if (dcnt_q == 10'd1023) begin
                                            disk_q <= D_SEC;
                                        end
                                    end
                                    D_SEC:  disk_q <= D_BUFW;
                                    D_BUFW: disk_q <= D_CMDW;
                                    D_CMDW: disk_q <= D_POLLW;
                                    D_BUFR: disk_q <= D_CMDR;
                                    D_CMDR: disk_q <= D_POLLR;
                                    D_POLLW, D_POLLR: begin
                                        if (st_ok && !bus_rdata[0]) begin
                                            dcnt_q <= '0;
                                            disk_q <= (disk_q == D_POLLW) ? D_BUFR : D_CHECK;
                                        end
                                    end
                                    D_CHECK: begin
                                        dcnt_q <= dcnt_q + 10'd1;
                                        if (dcnt_q == 10'd1023) begin
                                            cnt_op_q[13] <= cnt_op_q[13] + 32'd1;
                                            disk_q       <= D_IDLE;
                                        end
                                    end
                                    default: begin
                                        disk_q <= D_IDLE;
                                    end
                                endcase
                            end
                            default: begin
                            end
                        endcase
                    end
                    if (ev_done) begin
                        step_q <= '0;
                        cnt_q  <= '0;
                        op_q   <= op_t'(pick);
                    end
                    if (ev_err) begin
                        err_q <= err_q + 32'd1;
                        if (!e_valid_q) begin
                            e_valid_q  <= 1'b1;
                            e_kind_q   <= ekind_q;
                            e_status_q <= bus_status;
                            e_hdr_q    <= hdr_q;
                            e_addr_q   <= addr_q;
                            e_exp_q    <= (chk_q == C_DATA) ? exp_q : {24'h0, exp_st_q};
                            e_got_q    <= bus_rdata;
                        end
                    end
                    if (!stress_run || (ev_err && stop_on_err) || pick == 4'hF) begin
                        t_q <= T_IDLE;
                    end else begin
                        t_q <= T_PLAN;
                    end
                end

                default: begin
                    t_q <= T_IDLE;
                end
            endcase
        end
    end

    /* Host interface */

    always_comb begin
        probe_a = {
            4'h0, txmax_q,
            {t_q, op_q, disk_q, irq_q}, bus_dbg,
            ack_glitch_q, rst_short_q, rst_count_q, viol_q,
            e_hdr_q, e_got_q, e_exp_q, e_addr_q, e_status_q, e_kind_q, e_valid_q,
            cyc_q, rt_q, err_q, tx_q,
            bus_idle, stress_active,
            man_rdata_q, man_status_q, man_done_t_q
        };
        for (int k = 0; k < 14; k++) begin
            probe_b[32 * k +: 32] = cnt_op_q[k];
        end
        probe_b[479:448] = ovl_long_q;
        probe_b[495:480] = ovl_ev_q;
    end

    altsource_probe #(
        .sld_auto_instance_index ("YES"),
        .sld_instance_index      (0),
        .instance_id             ("T4A"),
        .probe_width             (464),
        .source_width            (200),
        .source_initial_value    ("0"),
        .enable_metastability    ("YES"),
        .lpm_type                ("altsource_probe")
    ) u_issp_a (
        .probe      (probe_a),
        .source     (src_a),
        .source_clk (clk),
        .source_ena (1'b1)
    );

    altsource_probe #(
        .sld_auto_instance_index ("YES"),
        .sld_instance_index      (1),
        .instance_id             ("T4B"),
        .probe_width             (496),
        .source_width            (1),
        .source_initial_value    ("0"),
        .enable_metastability    ("NO"),
        .lpm_type                ("altsource_probe")
    ) u_issp_b (
        .probe      (probe_b),
        .source     (src_b),
        .source_clk (clk),
        .source_ena (1'b1)
    );

    altsource_probe #(
        .sld_auto_instance_index ("YES"),
        .sld_instance_index      (2),
        .instance_id             ("T4C"),
        .probe_width             (480),
        .source_width            (26),
        .source_initial_value    ("C0000"),
        .enable_metastability    ("YES"),
        .lpm_type                ("altsource_probe")
    ) u_issp_c (
        .probe      (probe_c),
        .source     (src_c),
        .source_clk (dclk),
        .source_ena (1'b1)
    );

    /* Board indicators */

    function automatic logic [7:0] seg7(input logic [3:0] v);
        case (v)
            4'h0: seg7 = 8'b11000000;
            4'h1: seg7 = 8'b11111001;
            4'h2: seg7 = 8'b10100100;
            4'h3: seg7 = 8'b10110000;
            4'h4: seg7 = 8'b10011001;
            4'h5: seg7 = 8'b10010010;
            4'h6: seg7 = 8'b10000010;
            4'h7: seg7 = 8'b11111000;
            4'h8: seg7 = 8'b10000000;
            4'h9: seg7 = 8'b10010000;
            4'hA: seg7 = 8'b10001000;
            4'hB: seg7 = 8'b10000011;
            4'hC: seg7 = 8'b11000110;
            4'hD: seg7 = 8'b10100001;
            4'hE: seg7 = 8'b10000110;
            default: seg7 = 8'b10001110;
        endcase
    endfunction

    always_comb begin
        LEDR = {2'b00, pll_locked, irq_q, rst, stress_active, pdrv_q, oe_d, ack_d, req_d};
        HEX0 = seg7(tx_q[19:16]);
        HEX1 = seg7(tx_q[23:20]);
        HEX2 = seg7(tx_q[27:24]);
        HEX3 = seg7(tx_q[31:28]);
        HEX4 = seg7(err_q[3:0]);
        HEX5 = seg7(viol_q[3:0]);
    end
endmodule

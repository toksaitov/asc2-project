// Almurut II on the DE10-Lite: the chip in the FPGA, wired to a Raspberry
// Pi Pico that does the work of the RP2040 of a Tiny Tapeout board.
//
// The module holds the Tiny Tapeout top module unchanged and gives its
// pins the pins of the board:
//
//   port         board pin  Pico   job
//   clk50        P11               the 50 MHz clock of the board
//   pi_rst_n     W10        GP0    the reset, low to reset
//   pi_clk       W9         GP1    the clock from the Pico
//   uio[0]       W7         GP2    the bytes of the bus
//   uio[1]       V5         GP3
//   uio[2]       AA15       GP4
//   uio[3]       W13        GP5
//   uio[4]       AB13       GP6
//   uio[5]       Y11        GP7
//   uio[6]       W11        GP8
//   uio[7]       AA10       GP9
//   req          Y7         GP10   REQ, the strobe of the chip
//   pi_ack       Y6         GP11   ACK, the strobe of the Pico
//   pi_irq       Y5         GP12   IRQ, high while a device of the Pico asks
//   pi_stop      Y4         GP13   STOP from the Pico
//   pi_drives    Y3         GP14   high while the Pico drives uio
//   chip_drives  AA2        GP15   high while the chip drives uio
//   kbd_clk      W8         GP16   the clock line of the keyboard
//   kbd_data     Y8         GP17   the data line of the keyboard
//   pi_dbg_ck    V10        GP18   the clock of the debug port
//   pi_dbg_di    V9         GP19   the data input of the debug port
//   dbg_do       V8         GP20   the data output of the debug port
//
// The slide switch SW8 chooses the clock of the chip. Down: the clock of
// the board divided by 4, 12.5 MHz. Up: the clock from the Pico, as on a
// Tiny Tapeout board. The Pico's clock comes over a jumper wire, which
// picks up extra edges when the FPGA changes the eight wires of the bus at
// once. With it about one transaction in a million breaks off, and the
// chip repeats it; with the clock of the board none did in 21 million.
// The switch is moved only while the Pico holds the chip in reset.
//
// The slide switch SW7 puts the delays of a Tiny Tapeout board between the
// pins and the chip. Up: every input of the chip but its clock, and every
// output, comes 10 to 15 ns later (pin_delay.sv), as through the pads and
// the multiplexer of a Tiny Tapeout chip. Down: the pins are the chip's.
// This switch too is moved only while the Pico holds the chip in reset.
//
// STOP has two sources: the slide switch SW9 and the Pico. While SW9 is up
// or pi_stop is high, the CPU waits between two instructions. The Pico
// raises pi_stop for the commands `stop` and `step` of its monitor.
//
// The two keyboard lines and the two inputs of the debug port have
// pull-ups in the FPGA (asc2.qsf). Without their wires they read high: a
// keyboard that sends nothing, and a debug port whose clock never rises.
// The three wires of the debug port are on the other row of the header,
// the one with the 5 V and the 3.3 V pins of the board, at its first three
// pins (1, 3, and 5 of JP1).
//
// One guard is outside the chip. The FPGA and the Pico both drive the
// eight wires of the bus, and a mistake in either one would set two
// outputs against each other. The Pico raises pi_drives while it drives,
// and this module keeps its own drivers off during that time. The Pico
// does the same with chip_drives.
//
// The red LEDs show the wires: LEDR0 REQ, LEDR1 ACK, LEDR2 the chip
// drives, LEDR3 the Pico drives, LEDR4 FRAME, LEDR5 IRQ, LEDR6 STOP,
// LEDR7 the clock is the Pico's, LEDR8 the delays are on, LEDR9 the reset.
module asc2 (
    input  logic       clk50,
    input  logic       pi_rst_n,
    input  logic       pi_clk,
    inout  wire  [7:0] uio,
    output logic       req,
    input  logic       pi_ack,
    input  logic       pi_irq,
    input  logic       pi_stop,
    input  logic       pi_drives,
    output logic       chip_drives,
    input  logic       kbd_clk,
    input  logic       kbd_data,
    input  logic       pi_dbg_ck,
    input  logic       pi_dbg_di,
    output logic       dbg_do,
    input  logic [9:0] SW,
    output logic [9:0] LEDR
);
    logic [7:0] ui_in;
    logic [7:0] uo_out;
    logic [7:0] uio_out;
    logic [7:0] uio_oe;

    // The same signals at the pins, before or after their delays
    logic [7:0] ui_pins;
    logic [7:0] uio_late;
    logic       rst_n_late;
    logic [3:0] uo_pins;
    logic [7:0] uio_pins;
    logic       uio_oe_pins;

    logic [4:0] pll_clk;
    logic       is_late;

    logic [1:0] div_q;
    logic       board_clk;
    logic       is_pi_clk;
    logic       clk;

    logic       stop;
    logic       frame;

    // The free outputs of the chip have no wire, and seven switches are free
    logic       unused_pins;

    always_ff @(posedge clk50) begin
        div_q <= div_q + 2'd1;
    end

    tt_um_toksaitov_almurut2 chip (
        .ui_in   (ui_in),
        .uo_out  (uo_out),
        .uio_in  (uio_late),
        .uio_out (uio_out),
        .uio_oe  (uio_oe),
        .ena     (1'b1),
        .clk     (clk),
        .rst_n   (rst_n_late)
    );

    // 200 MHz for the delays of the pins
    altpll #(
        .intended_device_family ("MAX 10"),
        .operation_mode         ("NORMAL"),
        .pll_type               ("AUTO"),
        .inclk0_input_frequency (20000),
        .clk0_multiply_by       (4),
        .clk0_divide_by         (1),
        .clk0_duty_cycle        (50),
        .clk0_phase_shift       ("0"),
        .compensate_clock       ("CLK0"),
        .width_clock            (5),
        .lpm_type               ("altpll")
    ) pll (
        .inclk ({1'b0, clk50}),
        .clk   (pll_clk)
    );

    pin_delay #(
        .W (17)
    ) delay_in (
        .clk    (pll_clk[0]),
        .enable (is_late),
        .d      ({pi_rst_n, ui_pins, uio}),
        .q      ({rst_n_late, ui_in, uio_late})
    );

    // The chip drives all eight wires of the bus or none
    pin_delay #(
        .W (13)
    ) delay_out (
        .clk    (pll_clk[0]),
        .enable (is_late),
        .d      ({uo_out[3:0], uio_oe[0], uio_out}),
        .q      ({uo_pins, uio_oe_pins, uio_pins})
    );

    assign uio = (uio_oe_pins && !pi_drives) ? uio_pins : 8'bzzzz_zzzz;

    always_comb begin
        board_clk   = div_q[1];
        is_pi_clk   = SW[8];
        clk         = is_pi_clk ? pi_clk : board_clk;
        is_late     = SW[7];

        stop        = SW[9] || pi_stop;
        ui_pins     = {stop, kbd_data, kbd_clk, pi_dbg_di, 1'b1, pi_dbg_ck, pi_irq, pi_ack};

        req         = uo_pins[0];
        dbg_do      = uo_pins[1];
        chip_drives = uo_pins[2];
        frame       = uo_pins[3];

        LEDR        = {!pi_rst_n, is_late, is_pi_clk, stop, pi_irq, frame, pi_drives, chip_drives, pi_ack, req};

        unused_pins = &{uo_out[7:4], uio_oe[7:1], SW[6:0], pll_clk[4:1], 1'b0};
    end
endmodule

# Tiny Tapeout chip IO (SKY130, TTSKY26e), with GF180 notes

All facts retrieved 2026-11-21. "UNCERTAIN" marks things I could not confirm. "ADVICE" marks my own engineering suggestions, not TT statements.

## Summary table (SKY130)

| Item | Value | Source |
|---|---|---|
| Chip frame | Openframe (`top_level_macro: 'openframe_project_wrapper'`) | https://raw.githubusercontent.com/TinyTapeout/tinytapeout-sky-26e/main/config.yaml |
| Pad cell | sky130_ef_io_gpiov2_pad | https://tinytapeout.com/specs/gpio/ |
| IO supply on the demo board | 3.3 V | https://tinytapeout.com/specs/gpio/ |
| Pad supply range | 1.71 V to 5.5 V | https://tinytapeout.com/specs/gpio/ |
| 5 V tolerant inputs | No ("The input pins are not 5V tolerant") | https://tinytapeout.com/specs/gpio/ |
| Max input frequency | 66 MHz (SkyWater rating) | https://tinytapeout.com/specs/gpio/ |
| Max output frequency | 33 MHz (SkyWater rating) | https://tinytapeout.com/specs/gpio/ |
| Drive strength | 4 mA source/sink | https://tinytapeout.com/specs/gpio/ |
| Pad-to-tile-to-pad round trip | 20 ns worst, measured on TT3.5; TT datasheet source now says 17 ns | https://tinytapeout.com/specs/gpio/ and docs/product/facts.yaml in tt-support-tools |
| Pin-to-pin delay variance | less than 2 ns | https://tinytapeout.com/specs/gpio/ |
| Clock insertion delay, pad to project clk | up to 10 ns | https://tinytapeout.com/specs/clock/ |
| Core supply | 1.8 V (STA corner nom_tt_025C_1v80) | tech.py in tt-support-tools |

## 1. Pad type and pad configuration

* The chip uses the `sky130_ef_io_gpiov2_pad` macro for the IO pads.
  * Source: https://tinytapeout.com/specs/gpio/, retrieved 2026-11-21.
* TTSKY26e is an Openframe chip, not Caravel. The pinout table for "SKY130 Open Frame chips" applies (section 6 below).
  * Source: https://raw.githubusercontent.com/TinyTapeout/tinytapeout-sky-26e/main/config.yaml and https://github.com/TinyTapeout/tt-multiplexer/blob/main/docs/INFO.md (submodule commit 8fbe6cd used by the TTSKY26e repo), retrieved 2026-11-21.
* Pad settings are hard-coded in the TT Openframe wrapper (16-bit CONFIG per pad):
  * `TT_PAD_IN = 16'b0_00_00_0_0_0_0_0_0_0_0_001` (ui_in, clk, rst_n).
  * `TT_PAD_OUT = 16'b0_11_00_1_0_0_0_0_0_0_0_110` (uo_out).
  * `TT_PAD_INOUT = 16'b0_10_00_0_0_0_0_0_0_0_0_110` (uio).
  * Field map: [15] analog path, [14:13] output mode (00 off value 0, 01 off value 1, 10 dynamic, 11 always on), [10] inp_dis, [9] ib_mode_sel, [8] vtrip_sel, [7] slow_sel, [6] holdover, [5:3] analog controls, [2:0] dm.
  * Source: ol2/tt_top/openframe_project_wrapper.v and rtl/tt_gpio.v in https://github.com/TinyTapeout/tt-multiplexer at commit 8fbe6cd3ebc3808a4cb3d589d1a2728c46a7054c, retrieved 2026-11-21 (saved at tt/ttmux/).
* What those bits mean, per the SkyWater gpio user guide:
  * dm = 001: "Configured as Input only". dm = 110: "Strong pull-up" and "Strong pull-down" (push-pull).
  * vtrip_sel = 0: "the input buffer functions as a CMOS input buffer". ib_mode_sel = 0: thresholds referenced to VDDIO.
  * slow = 0: default (fast) edge rate. The table gives "Strong-Fast (2-12ns)" for slow = 0.
  * Source: https://skywater-pdk.readthedocs.io/en/main/contents/libraries/sky130_fd_io/docs/user_guide.html, retrieved 2026-11-21.
* So, on the TT chip:
  * ui_in, clk, rst_n: plain CMOS inputs, no pull-up or pull-down. A floating input floats.
  * uo_out: always-on push-pull outputs, input buffer disabled, fast slew.
  * uio: push-pull when the project sets uio_oe = 1; input when uio_oe = 0. The input buffer stays on, so uio_in reads back the pin even while driving.
  * INFERENCE from the CONFIG values above. Not stated in words by TT.

## 2. Voltage, frequency and drive

* IO supply 1.71 V to 5.5 V for the pad. "The demo board provides 3.3V IO supply voltage. The input pins are not 5V tolerant."
  * Source: https://tinytapeout.com/specs/gpio/, retrieved 2026-11-21.
* ETR demo board: "All the I/O expects 3.3V logic levels".
  * Source: https://tinytapeout.com/guides/get-started-demoboard-etr/, retrieved 2026-11-21.
* Max input frequency 66 MHz, max output frequency 33 MHz, drive 4 mA source/sink.
  * Source: https://tinytapeout.com/specs/gpio/, retrieved 2026-11-21.
* TT comments on these limits (same page):
  * Above 66 MHz inputs "can start to distort, mostly because falling edges propagate faster than rising edges". About 100 MHz has worked "but it shouldn't be relied upon".
  * "you can output a 33 MHz square wave and it still looks acceptable - albeit with some asymmetry - if the load isn't large." A QSPI Pmod clock at 32 MHz worked.
* Measured sink and source IV curves are on the GPIO page (single die, about 22 C, "may not be representative of future batches").
  * Source: https://tinytapeout.com/specs/gpio/, retrieved 2026-11-21.

## 3. Mux path delay (input and output)

* "After Tiny Tapeout 3.5 silicon was received and tested the worst round trip latency was measured to be 20 ns".
* "Delay variance between different IO pins has been measured at less than 2ns."
  * Source: https://tinytapeout.com/specs/gpio/, retrieved 2026-11-21. Measured on TT3.5 (Caravel era), single die.
* The TT product datasheet source lists sky130A "Pad-to-tile round-trip latency" as 17 ns, with the note "website has 20 ns, measured on TT3.5".
  * Source: docs/product/facts.yaml in https://github.com/TinyTapeout/tt-support-tools (main, commit 0922544), cloned 2026-11-21.
* Clock: "We expect a latency (insertion delay) of up to 10 nanoseconds between the chip's I/O pad and your project's clock."
  * Source: https://tinytapeout.com/specs/clock/, retrieved 2026-11-21.
* UNCERTAIN: TT does not publish separate one-way input and output delays. Taking the clock figure as the input path, the output path is roughly the rest of the 17-20 ns round trip. Treat both one-way paths as up to about 10 ns.
* The path, from the RTL:
  * Input: pad, controller input buffer (gated per spine side), spine, mux row decode, zeroing buffer (zbuf), project pin.
  * Output: project pin, mux4 per group of 4 tiles, tri-state buffer onto the row bus, tri-state buffer onto the spine, controller mux, pad.
  * Source: rtl/tt_mux.v and rtl/tt_ctrl.v in tt-multiplexer at commit 8fbe6cd, retrieved 2026-11-21.
* These delays are outside your tile. The LibreLane STA for your tile does not include them.
  * Source: the default SDC only sets generic input/output delays as a percent of CLOCK_PERIOD, https://raw.githubusercontent.com/librelane/librelane/3.0.14/librelane/scripts/base.sdc, retrieved 2026-11-21.

## 4. How clk and rst_n reach the project

* "Internally, both the clk and rst_n pins are handled like any other input pins."
  * Source: https://tinytapeout.com/specs/gpio/ and https://tinytapeout.com/specs/clock/, retrieved 2026-11-21.
* "Internally, there's no difference between u_clk, u_rst_n, and ui pins. They are all just bits in the pad_ui_in bus."
  * Source: https://github.com/TinyTapeout/tt-multiplexer/blob/main/docs/INFO.md, retrieved 2026-11-21.
* In the wrapper, the 10-bit input word is unpacked as `{ uio_in, ui_in, rst_n, clk } = iw`.
  * Source: rtl/tt_user_module.v.mak in tt-multiplexer at commit 8fbe6cd, retrieved 2026-11-21.
* For every design that is not selected, "clk, rst_n, ui_in, uio_in are all tied to zero". So an unselected design sits in reset (rst_n = 0) with clk = 0.
  * Source: https://github.com/TinyTapeout/tt-multiplexer/blob/main/docs/INFO.md, retrieved 2026-11-21.
  * The original IO spec says "Disabled modules are guaranteed to have '0' as input ( so prefer active low reset !!! )".
  * Source: docs/tt_ios_spec.md in tt-multiplexer, retrieved 2026-11-21.
* The demo board's RP2350 "is connected not only to the project MUX but to all the ASIC I/O as well as the clock and reset pins." It generates the project clock (for example `tt.clock_project_PWM(10e6)`) and can hold the project in reset (`start_in_reset`, `tt.reset_project`).
  * Source: https://tinytapeout.com/guides/get-started-demoboard-etr/, retrieved 2026-11-21.
* The clock page says the demo board clock can be set "between 1 Hz and 66.5 MHz", generated by PWM or PIO.
  * Source: https://tinytapeout.com/specs/clock/, retrieved 2026-11-21.
  * UNCERTAIN: that page still describes the older RP2040 board and the Caravel pin (mprj_io[6], QFN pin 37). On the Openframe chip the clock is mprj_io[14], QFN pin 50 (see section 6). Treat the clock page pin numbers as stale.
* No synchronizer is added by TT on rst_n or ui_in.
  * Source: the input path in rtl/tt_mux.v is buffers only, tt-multiplexer at commit 8fbe6cd, retrieved 2026-11-21.

## 5. ena, power gating and uio_oe

ena:
* Template comment: "always 1 when the design is powered, so you can ignore it".
  * Source: src/project.v in https://github.com/TinyTapeout/ttsky-verilog-template (commit a42e666), cloned 2026-11-21.
* HDL page wording: "will go high when the design is enabled".
  * Source: https://tinytapeout.com/hdl/important/, retrieved 2026-11-21.
* In the RTL, the project's `ena` and its power-gate control `pg_ena` are driven from the same select signal (`l_ena`).
  * Source: rtl/tt_mux.v (`um_ena` and `um_pg_ena` both from `l_ena`) and rtl/tt_user_module.v.mak (`tt_pg_vdd_I ... .ctrl(pg_ena)`), tt-multiplexer at commit 8fbe6cd, retrieved 2026-11-21.
* Consequences (INFERENCE from the RTL):
  * A project is powered only while it is selected and the global enable is on.
  * Deselecting the project removes power. All state is lost.
  * After selection, flops start random. Hold rst_n low with the clock running before use.
* SKY130 power-gate macros exist in several sizes (tt_pg_1v8_hp_1/2/4, tt_pg_1v8_ll_1/2/4). TT picks the one used.
  * Source: Sky130Tech.mux_macros in tech.py, tt-support-tools clone.
  * UNCERTAIN: the current limit of the power gate for an 8x2 design. Not documented on the pages I read.

uio_oe:
* Per bit, active high: "0=input, 1=output".
  * Source: src/project.v and test/tb.v in the template clone.
* When the project is not selected, its outputs are disconnected: "uo_out, uio_oe, uio_out are disconnected from the spine (the tristate buffer output enable is disabled)".
  * Source: https://github.com/TinyTapeout/tt-multiplexer/blob/main/docs/INFO.md, retrieved 2026-11-21.
* When no design on a side drives the spine, the controller pulls uo_out, uio_out and uio_oe to 0 ("Floating net prevention").
  * Source: rtl/tt_ctrl.v in tt-multiplexer at commit 8fbe6cd, retrieved 2026-11-21.
* So with no project active: uo_out pads drive 0, and uio pads are inputs (high impedance, no pull). INFERENCE from the two sources above.

## 6. Pin map for the planned CPU bus (SKY130 Openframe QFN64)

Source: "SKY130 Open Frame chips" table, https://github.com/TinyTapeout/tt-multiplexer/blob/main/docs/INFO.md, retrieved 2026-11-21.

| Planned use | TT signal | mprj_io | QFN64 pin |
|---|---|---|---|
| ACK in | ui[0] | 0 | 31 |
| IRQ in | ui[1] | 1 | 32 |
| debug in | ui[2] | 2 | 33 |
| UART RX (left free) | ui[3] | 3 | 34 |
| debug in | ui[4] | 4 | 35 |
| (free) | ui[5], ui[6] | 5, 6 | 36, 37 |
| (free) | ui[7] | 13 | 48 |
| project clock | u_clk | 14 | 50 |
| project reset | u_rst_n | 15 | 51 |
| data bus D0..D2 | uio[0..2] | 16..18 | 53, 54, 55 |
| data bus D3..D7 | uio[3..7] | 19..23 | 57..61 |
| REQ out | uo[0] | 24 | 62 |
| debug out | uo[1] | 25 | 2 |
| (free) | uo[2], uo[3] | 26, 27 | 3, 4 |
| UART TX (left free) | uo[4] | 28 | 5 |
| (free) | uo[5..7] | 29..31 | 6..8 |
| mux control | ctrl_ena, ctrl_sel_inc, ctrl_sel_rst_n | 38, 39, 40 | 22, 24, 25 |

* TT's recommended UART-to-USB pins are ui_in[3] RX and uo_out[4] TX. The alternative pair is ui_in[1] RX and uo_out[0] TX.
  * Source: https://tinytapeout.com/specs/pinouts/#uart-to-usb, retrieved 2026-11-21.
  * The plan uses ui_in[1] (IRQ) and uo_out[0] (REQ), which is the alternative UART pair. It keeps the primary pair free, so it is fine. Just do not enable the alternative UART mapping in firmware.
* The demo board DIP switches and the INPUT Pmod can also drive ui_in. TT's troubleshooting says to keep the INPUT Pmod unconnected and the switches "off". Firmware `mode = ASIC_RP_CONTROL` makes the RP2350 drive the inputs.
  * Source: https://tinytapeout.com/guides/get-started-demoboard-etr/, retrieved 2026-11-21.
  * Board wiring details belong in the rp/ notes.

ADVICE for the REQ/ACK bus (mine, not TT):
* ACK and IRQ are asynchronous to clk. Pass each through a 2-flop synchronizer before use.
* With no project active, uo_out = 0 and uio is high impedance. Make REQ active high so 0 means idle, and let the RP2350 hold a defined level on uio (pulls) when neither side drives.
* Put data on uio at least one clock before raising REQ. Pin-to-pin skew is under 2 ns, so one clock period of setup covers it at any clock the RP2350 can generate.
* Change uio_oe only in protocol phases where the RP2350 is not driving, to avoid contention. Remember the oe change also travels through the mux (up to about 10 ns, UNCERTAIN).
* Do not rely on any state surviving a deselect. Reset after every select.

## 7. Known IO issues on silicon

* I found no official TT report of an IO defect on the SKY130 Openframe chips (TTSKY25a onward).
  * Searched: tinytapeout.com/specs/gpio, /specs/clock, /chips/ttsky25a, /chips/ttsky25b, /news/, and a web search, 2026-11-21.
  * UNCERTAIN: bring-up issues may be discussed only on Discord.
* Caveats that TT does state:
  * The latency and IV data come from a single die at about 22 C and "may not be representative of future batches".
  * The 20 ns figure is from TT3.5 (Caravel). No Openframe-specific latency number is published on the website.
  * Output above about 33 MHz shows duty-cycle asymmetry; inputs above 66 MHz distort.
  * Source: https://tinytapeout.com/specs/gpio/, retrieved 2026-11-21.
* Documentation drift to watch: the clock page names mprj_io[6] / QFN pin 37 and an RP2040, which match older Caravel chips and the older board, not TTSKY26e.
  * Source: https://tinytapeout.com/specs/clock/ vs https://github.com/TinyTapeout/tt-multiplexer/blob/main/docs/INFO.md, retrieved 2026-11-21.

## 8. GF180 equivalents (brief)

* Values in the TT product datasheet source for gf180mcuD: IO voltage "3.30/5.00" V, input 66 MHz, output 66 MHz, drive 4 mA, round-trip latency 20 ns, pin-to-pin variance 2 ns.
  * Source: docs/product/facts.yaml in tt-support-tools (main, commit 0922544), cloned 2026-11-21.
  * UNCERTAIN: that file says it holds "Only values with no machine-readable source". These look hand-entered, not measured. No GF IO spec page exists on tinytapeout.com that I could find.
* Core supply 3.3 V (gf180mcu_fd_sc_mcu7t5v0 library at 3v30, corner nom_tt_025C_3v30).
  * Source: GF180MCUDTech in tech.py, tt-support-tools clone.
* Custom GF180 pad frame. Pins: uo[0..7] QFN 9-16, uio[0..7] QFN 33-40, ui[0..7] QFN 41-48, u_rst_n 49, u_clk 50, mux control QFN 1-3.
  * Source: "GF180mcuD custom pad frame" table, https://github.com/TinyTapeout/tt-multiplexer/blob/main/docs/INFO.md, retrieved 2026-11-21.
* GF180 dies are chip-on-board: "the die itself is wire bonded and epoxied to the breakout board and therefore cannot be removed."
  * Source: https://tinytapeout.com/specs/pcb-etr/, retrieved 2026-11-21.
* GF180 tile: 1x1 = 346.64 x 160.72 um (template comment "about 340x160 uM"). Same mux idea, 8 columns x 20 rows = 160 tiles.
  * Source: tech/gf180mcuD/tile_sizes.yaml in tt-support-tools; https://raw.githubusercontent.com/TinyTapeout/ttgf-verilog-template/main/info.yaml; GF shuttle maps on app.tinytapeout.com, retrieved 2026-11-21.

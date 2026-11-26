# TT dev board (demoboard v3) and how the RP2350 controls the chip

All facts retrieved 2026-11-21 unless a line says otherwise.
Repository paths are relative to [asc2-kb/](./).
"v3.2" and "v3.3" are the two fabricated revisions of the TT ETR demoboard v3.
"computed" marks a number I derived. It is not stated in a source.
"inference" marks a conclusion not stated in a source.
Pad-level netlists behind this file: tt-devboard-netlists.txt (same folder).

## 0. Key facts and risks (read first)

* The current board is the TT ETR demoboard v3 with an RP2350B. Two revisions are in the field: v3.2 and v3.3. Firmware detects both. [src: repos/tt-micropython-firmware/src/ttboard/boot/demoboard_detect.py; https://github.com/TinyTapeout/pcb-files/tree/main/ETR/demoboard, retrieved 2026-11-21]
* The GPIO map in the earlier notes is correct on both revisions: GPIO17-24 = ui_in[0..7], GPIO25-32 = uio[0..7], GPIO33-40 = uo_out[0..7], GPIO16 = clk, GPIO14 = rst_n. [src: v3.2 and v3.3 .kicad_pcb pad nets, tt-devboard-netlists.txt, retrieved 2026-11-21]
* Mux control: GPIO0 = ctrl_ena, GPIO1 = ctrl_sel_rst_n, GPIO2 = ctrl_sel_inc. [src: same, retrieved 2026-11-21]
* REQ = uo_out[0] = GPIO33 is the GPIO right after uio[7] = GPIO32. The planned PIO IN window (base GPIO25, 9 pins) works. [src: same, retrieved 2026-11-21]
* With PIO GPIOBASE = 16 one PIO block sees GPIO16-47 as its pins 0-31. clk, all ui_in, all uio, all uo_out fit in one window. GPIO0-2 and GPIO14 do not. Drive those from SIO. [src: RP2350 datasheet PIO GPIOBASE register; pico-sdk pio.c, retrieved 2026-11-21]
* The clock and reset nets changed between revisions. v3.2: 47 Ohm in series on clk and 100 nF on rst_n. v3.3: clk is a direct trace, rst_n is driven by a Schmitt inverter through 1 kOhm. [src: tt-devboard-netlists.txt sections 1 and 2, retrieved 2026-11-21]
* Every uo_out pin drives a 7-segment LED through a bridged solder jumper and 510 Ohm. REQ lights segment A. Load is about 1.4-2.5 mA per high pin (computed). [src: repos/tt-demo-pcb/doc/tt-etr-dbv3.pdf, retrieved 2026-11-21]
* Every ui_in pin has a DIP switch to +3V3 through 1 kOhm. A switch left ON fights the RP on ACK/IRQ. Keep all DIP switches OFF. [src: same PDF, retrieved 2026-11-21]
* No pull resistors on uio, ui_in or uo_out on the demoboard or on the SKY130 breakout. [src: tt-devboard-netlists.txt, retrieved 2026-11-21]
* RP2350 stepping on the board is not documented. BOM says only "RP2350B". Erratum E9 (input latching at about 2.2 V) affects A2 only. A4 is the production stepping. Check the part marking on the real board. [src: https://github.com/TinyTapeout/pcb-files ETR/demoboard/v3p3/bom/tinytapeout-demo_bom_jlc.csv; RP2350 datasheet Table 7 and D.5.1, retrieved 2026-11-21]
* Custom pico-sdk UF2 firmware is possible: hold BOOT, plug USB, copy the UF2. Restore with tt-demo-rp2350-v3.1.1.uf2. [src: repos/tt-micropython-firmware/README.md "Installation", retrieved 2026-11-21]
* No official C firmware for the v3 board was found. No official RP2350 RAM emulation was found. spi-ram-emu is RP2040-only. [src: section 9 below, retrieved 2026-11-21]

## 1. Which board ships with SKY130 and GF180 chips

* The ETR guide says the management MCU is "an RP2350B on the latest demoboards". [src: https://tinytapeout.com/guides/get-started-demoboard-etr/, retrieved 2026-11-21]
* The same demoboard serves SKY130 and GF180. Only the breakout differs. The TT archive holds one ETR demoboard folder plus breakout-skyqfn-v3p2, breakout-gf-cob-v0p2 and breakout-ihp-qfn folders. [src: https://github.com/TinyTapeout/pcb-files/tree/main/ETR, retrieved 2026-11-21]
* For GF shuttles the die is wire bonded and epoxied to the breakout and cannot be removed. [src: https://tinytapeout.com/specs/pcb-etr/, retrieved 2026-11-21]
* Store pages for the TTSKY25b kit and the TTGF0p2 kit link the tt-demo-pcb repo. They state no board revision and no MCU. [src: https://store.tinytapeout.com/products/TTSKY25b-Development-Kit-p849160072 ; https://store.tinytapeout.com/products/TTGF0p2-Development-Kit-P2-edition-p841828501, retrieved 2026-11-21]
* v3.2: schematic title block "Tiny Tapeout Demoboard v3", Rev 3.2, dated 2025-11-12. Commit d830790c "v3.2 as prototyped" on 2025-12-01. Production files in pcb-files ETR/demoboard/v3p2. [src: repos/tt-demo-pcb/doc/tt-etr-dbv3.pdf; https://github.com/TinyTapeout/tt-demo-pcb/commits/main, retrieved 2026-11-21]
* v3.3: Rev 3.3, dated 2026-06-23. Commit ecb636ac "Reworked manual clocking circuit, Iref current source for analog, TVS diodes on analog lines". Production files (gerbers, BOM, PDF) added to pcb-files on 2026-09-19, commit ca5fd724. [src: repos/tt-demo-pcb/tinytapeout-demo.kicad_sch; https://github.com/TinyTapeout/pcb-files/tree/main/ETR/demoboard/v3p3, retrieved 2026-11-21]
* The tt-demo-pcb README and doc/tt-etr-dbv3.pdf still describe v3.2. The KiCad sources at HEAD (bbf3dd1f, 2026-08-29) are v3.3. [src: repos/tt-demo-pcb/README.md, retrieved 2026-11-21]
* Firmware added v3.3 support on 2026-08-05 (commit 13fae157). Release v3.0.8 (2026-08-11) and later include it. [src: https://github.com/TinyTapeout/tt-micropython-firmware/commits/main/src/ttboard/boot/demoboard_detect.py, retrieved 2026-11-21]
* Which revision our chip will ship with is unknown. Inference: v3.3 or later. Design the RP firmware for both.
* Stale official pages: /specs/pcb/ and /guides/get-started-demoboard/ describe the RP2040 board of TT04-TT08. /specs/pinouts/ still lists the RP2040 GPIO21-24 for spi-ram-emu on uio[0..3]. [src: https://tinytapeout.com/specs/pcb/ ; https://tinytapeout.com/specs/pinouts/, retrieved 2026-11-21]

## 2. Power and voltages

* USB-C 5 V -> F1 350 mA polyfuse -> FB1 120 Ohm ferrite -> VBUS -> U1 TLV1117LV33 LDO -> +3V3. Same on both revisions. [src: repos/tt-demo-pcb/doc/tt-etr-dbv3.pdf "Power"; tt-devboard-netlists.txt, retrieved 2026-11-21]
* +3V3 feeds every RP2350 IOVDD pin, QSPI_IOVDD, ADC_AVDD, USB_OTP_VDD, VREG_VIN, the flash, all PMOD VCC pins and the breakout 3v3 pins. [src: tt-devboard-netlists.txt net +3V3, retrieved 2026-11-21]
* RP2350 core (DVDD) is +1V1 from its internal switching regulator with L1 3.3 uH. [src: same, net +1V1, retrieved 2026-11-21]
* SKY130 breakout: chip VDDIO = +3V3 through 0 Ohm links. VDDA = +3V3 through 0 Ohm. Core VCCD = 1.8 V from U1 AP2112K-1.8 fed from +3V3. The second regulator U2 is DNP. [src: repos/breakout-pcb/breakout-sky-qfn/breakout-sky-qfn.kicad_pcb, tt-devboard-netlists.txt section 3, retrieved 2026-11-21]
* GF180 COB breakout: VDD_CORE, VDD_IO and VAA are all +3V3 through 0 Ohm links. Both regulators are DNP. [src: repos/breakout-pcb/breakout-gf-cob/breakout-gf-cob.kicad_pcb, retrieved 2026-11-21]
* Store, TTGF0p2 P2 kit: "Each version runs at 3.3v by default". Using other IO and core voltages on P2 "will result in chip death". [src: https://store.tinytapeout.com/products/TTGF0p2-Development-Kit-P2-edition-p841828501, retrieved 2026-11-21]
* No level shifting anywhere. RP pads and chip pads share the +3V3 rail. They connect by plain copper through two Hirose DF12 connectors (60-pin and 50-pin). [src: tt-devboard-netlists.txt sections 1-3, retrieved 2026-11-21]
* "All the I/O expects 3.3V logic levels." Typical draw about 180-200 mA from 5 V. The board can be back-powered with 3V3 on a header pin. [src: https://tinytapeout.com/guides/get-started-demoboard-etr/, retrieved 2026-11-21]
* The RP2350 cannot switch or sequence chip power. Chip and RP power up together from the same LDO (inference from the netlists).
* Chip IO pad (SKY130): sky130_ef_io_gpiov2_pad, 4 mA drive, IO supply 1.71-5.5 V, not 5 V tolerant, outputs rated 33 MHz, inputs rated 66 MHz. [src: https://tinytapeout.com/specs/gpio/, retrieved 2026-11-21]
* RP2350 IO at IOVDD 3.3 V: VIH min 2.0 V. VIL max 0.8 V. Hysteresis min 0.2 V with Schmitt on. VOH min 2.62 V and VOL max 0.5 V at the set drive current. Pull-up 32-86 kOhm. Pull-down 36-113 kOhm. Max total IOVDD source current 100 mA. [src: RP2350 datasheet Table 1683, https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf (build 19/11/2026, f6dffdf503a8), retrieved 2026-11-21]
* RP2350 pad reset state (PADS_BANK0 GPIOx): ISO=1, OD=0, IE=0, DRIVE=4 mA, PUE=0, PDE=1, SCHMITT=1, SLEWFAST=0. So every GPIO is hi-Z with a weak pull-down until firmware runs. [src: RP2350 datasheet Table 885, retrieved 2026-11-21]

## 3. Full RP2350 GPIO map

Source for the whole table: pad nets of the v3.2 PCB (tt-demo-pcb commit 0277545d) and the v3.3 PCB (repos/tt-demo-pcb/tinytapeout-demo.kicad_pcb), in tt-devboard-netlists.txt; RP2350 datasheet function table; retrieved 2026-11-21.

| GPIO | v3.2 | v3.3 | Other loads on the net |
|---|---|---|---|
| 0 | ctrl_ena | ctrl_ena | 10k pull-up (R30 / R38). Breakout 1k pull-down. DNP header J8.5 / J7.5 |
| 1 | ctrl_sel_rst_n | ctrl_sel_rst_n | 10k pull-up (R31 / R39). Breakout 1k pull-down. DNP header |
| 2 | ctrl_sel_inc | ctrl_sel_inc | 5.1k pull-up (R29 / R37). No breakout pull. DNP header |
| 3-10 | mng0-mng7 | mng0-mng7 | Go to breakout J3 pins 42, 40, 36, 30, 26, 22, 18, 16. GPIO10 (mng7) has 10k pull-down (R63 / R33). On the SKY breakout only mng1/mng2 go anywhere (DNP EEPROM) |
| 11 | RP LED | RP LED | 510 Ohm + yellow LED D5 to GND |
| 12 | NC | IREFPWM | v3.3: drives an RC filter into an op-amp current source. Output goes only to DNP header J11. Firmware holds it high (source off) |
| 13 | NC | NC | |
| 14 | rst_n (project_rst) | rst_n | See section 4 row rst_n |
| 15 | NC | man_clk input | v3.3: driven by Schmitt inverter U2 output. Never drive this GPIO |
| 16 | clk | clk | See section 4 row clk. Alt fn PWM0 A |
| 17-24 | ui_in[0..7] | ui_in[0..7] | DIP switch + Input PMOD |
| 25-32 | uio[0..7] | uio[0..7] | Bidir PMOD only |
| 33-40 | uo_out[0..7] | uo_out[0..7] | 7-seg + Output PMOD. GPIO40 is also ADC0 |
| 41-45 | ADC1-5 | ADC1-5 | To DNP analog header (J16 / J12) |
| 46-47 | NC | NC | |
| QSPI | W25Q32JV flash | W25Q32JV flash | QSPI_SS also has 10k pull-up and the BOOT button (1k to GND) |
| RUN | DNP header J7 | DNP header J8 | No reset button on the board |
| SWD | DNP header J15 | DNP header J17 | |

## 4. Chip pin table

Bus roles are from the project plan. QFN pins and DF12 pins are for the SKY130 breakout. Sources: tt-devboard-netlists.txt sections 1-3; RP2350 datasheet GPIO function table (lines for GPIO16, 20, 37); retrieved 2026-11-21.

| Chip pin | RP2350 GPIO | Our bus role | QFN64 / DF12 | Everything else on that net |
|---|---|---|---|---|
| clk | 16 (PWM0 A, PIO) | project clock from RP | 50 / J1.57 | v3.2: 47 Ohm R15 between GPIO16 and the chip. 10k R14 to the manual-clock network (button SW3 to 3V3, 1 uF across it, 47k to GND). JP9 to DNP J9.3. v3.3: direct trace, no resistor. JP1 (bridged) to DNP J9.3. Breakout TP2 pad |
| rst_n | 14 | project reset from RP | 51 / J1.53 | v3.2: 100 nF C19 to GND. 1k R22 to a node with 10k R19 pull-up and button SW4 to GND. v3.3: 1k R23 from Schmitt inverter U3 output. U3 holds rst_n high unless SW4 is pressed. Breakout TP1 pad |
| ctrl_ena | 0 | mux enable | 22 / J3.46 | 10k pull-up on demoboard. 1k pull-down on breakout (SKY R9, GF R11) |
| ctrl_sel_rst_n | 1 | mux counter reset | 25 / J3.43 | 10k pull-up on demoboard. 1k pull-down on breakout (SKY R10 via R5 0 Ohm, GF R10) |
| ctrl_sel_inc | 2 | mux counter clock | 24 / J3.47 | 5.1k pull-up on demoboard. Nothing on breakout |
| ui_in[0] | 17 (PIO) | ACK (RP -> chip) | 31 / J1.31 | DIP SW1 pos 1 (1k to 3V3 when ON). Input PMOD pin 1. DNP J10.1. SKY breakout TP4 pad |
| ui_in[1] | 18 | IRQ (RP -> chip) | 32 / J1.33 | DIP pos 2. Input PMOD pin 2. DNP J10.2 |
| ui_in[2] | 19 | debug port clock | 33 / J1.37 | DIP pos 3. Input PMOD pin 3. DNP J10.3 |
| ui_in[3] | 20 (UART1 TX) | free | 34 / J1.39 | DIP pos 4. Input PMOD pin 4. DNP J10.4 |
| ui_in[4] | 21 | debug port data in | 35 / J1.41 | DIP pos 5. Input PMOD pin 7 |
| ui_in[5] | 22 | keyboard clock | 36 / J1.45 | DIP pos 6. Input PMOD pin 8 |
| ui_in[6] | 23 | keyboard data | 37 / J1.47 | DIP pos 7. Input PMOD pin 9 |
| ui_in[7] | 24 | STOP (RP -> chip) | 48 / J1.49 | DIP pos 8. Input PMOD pin 10 |
| uio[0] | 25 | data bit 0, PIO IN pin 0 | 53 / J1.5 | Bidir PMOD pin 1. DNP J6.1 |
| uio[1] | 26 | data bit 1 | 54 / J1.7 | Bidir PMOD pin 2. DNP J6.2. SKY breakout TP3 pad |
| uio[2] | 27 | data bit 2 | 55 / J1.9 | Bidir PMOD pin 3. DNP J6.3 |
| uio[3] | 28 | data bit 3 | 57 / J1.13 | Bidir PMOD pin 4. DNP J9.1 |
| uio[4] | 29 | data bit 4 | 58 / J1.15 | Bidir PMOD pin 7 |
| uio[5] | 30 | data bit 5 | 59 / J1.17 | Bidir PMOD pin 8 |
| uio[6] | 31 | data bit 6 | 60 / J1.21 | Bidir PMOD pin 9 |
| uio[7] | 32 | data bit 7, PIO IN pin 7 | 61 / J1.23 | Bidir PMOD pin 10 |
| uo_out[0] | 33 | REQ (chip -> RP), PIO IN pin 8 | 62 / J3.19 | 7-seg segment A via JP1 (v3.2) / JP2 (v3.3) + 510 Ohm. Output PMOD pin 1. DNP J4.1 |
| uo_out[1] | 34 | debug port data out | 2 / J3.17 | Segment B via jumper + 510 Ohm. Output PMOD pin 2 |
| uo_out[2] | 35 | UIO_OE (chip -> RP) | 3 / J3.15 | Segment C. Output PMOD pin 3 |
| uo_out[3] | 36 | FRAME (chip -> RP) | 4 / J3.13 | Segment D. Output PMOD pin 4 |
| uo_out[4] | 37 (UART1 RX) | free, high | 5 / J3.9 | Segment E. Output PMOD pin 7 |
| uo_out[5] | 38 | free, low | 6 / J3.7 | Segment F. Output PMOD pin 8 |
| uo_out[6] | 39 | free, low | 7 / J3.5 | Segment G. Output PMOD pin 9 |
| uo_out[7] | 40 (ADC0) | free, low | 8 / J3.3 | Segment DP. Output PMOD pin 10 |

* ui_in[3] = GPIO20 has UART1 TX and uo_out[4] = GPIO37 has UART1 RX (function F2). The hardware UART1 lines up with the TT UART pinout. [src: RP2350 datasheet GPIO function table; https://tinytapeout.com/specs/pinouts/, retrieved 2026-11-21]
* The chip QFN pin numbers above match the SKY130 open-frame pinout (u_rst_n = QFN 51). [src: https://github.com/TinyTapeout/tt-multiplexer/blob/main/docs/INFO.md, retrieved 2026-11-21]

## 5. DIP switches, buttons, 7-segment, LEDs, PMODs

Source for this section unless noted: repos/tt-demo-pcb/doc/tt-etr-dbv3.pdf (v3.2), https://github.com/TinyTapeout/pcb-files/blob/main/ETR/demoboard/v3p3/tinytapeout-demo.pdf (v3.3), tt-devboard-netlists.txt; retrieved 2026-11-21.

* DIP SW1 is a CTS 193-8MSR (alt 76SB08ST). Each position has its own 1 kOhm from +3V3 to one switch side. The other side is the ui_in net. ON = ui_in pulled hard to 3V3 through 1k. OFF = open. No pull-down on the board. Position 1 = ui_in[0].
* An ON switch against an RP output driving low costs 3.3 mA (computed: 3.3 V / 1 kOhm). The RP still wins (VOL max 0.5 V at 4 mA). The RP output drive must be at least 4 mA.
* Stock firmware: in ASIC_RP_CONTROL mode it reads each ui_in at start. If one reads high (switch ON) it leaves that pin as an input and logs contention. [src: repos/tt-micropython-firmware/src/ttboard/pins/pins.py begin_asiconboard, retrieved 2026-11-21]
* BOOT button SW2: pulls RP2350 QSPI_SS low through 1k R13. 10k pull-up R12. Hold while plugging USB to enter BOOTSEL.
* Manual clock button SW3. v3.2: button to 3V3, 1 uF across it, 47k to GND, then 10k into the clk net. v3.3: 10k/1k/1 uF debounce into Schmitt inverter U2, output to GPIO15 only. The RP reads it and calls clock_project_once(). [src: repos/tt-micropython-firmware/src/ttboard/pins/manual_clock.py, retrieved 2026-11-21]
* Project reset button SW4. v3.2: 10k pull-up, button to GND, 1k into rst_n, 100 nF on rst_n. v3.3: debounce into Schmitt inverter U3, U3 output drives rst_n through 1k. The v3.3 schematic note: rst_n "sits HIGH after inverter unless button press or driven LOW by RP2".
* v3.3 BOM lists the inverters as NC7SV14P5X. The schematic symbol says NC7SP14P5X. [src: pcb-files ETR/demoboard/v3p3/bom/tinytapeout-demo_bom_jlc.csv, retrieved 2026-11-21]
* 7-segment U2 (v3.2) / U4 (v3.3): Wurth 157112V12700, common cathode to GND, Vf typ 2 V, max 2.6 V. uo_out[0..7] -> segments A, B, C, D, E, F, G, DP. Each goes through a SolderJumper_2_Bridged (closed by default) then 510 Ohm.
* Segment current with uo_out high: about 2.5 mA at Vf 2.0 V, 1.4 mA at Vf 2.6 V (computed: (3.3 V - Vf) / 510 Ohm). Cut the jumper to remove the load from a pin.
* LEDs: D3 on VBUS (1.2k), D4 on +3V3 (510), D5 on GPIO11 (510).
* PMODs: three plain female 2x6 Digilent-numbered hosts. Input PMOD = ui_in, Bidir PMOD = uio, Output PMOD = uo_out. Pins 1-4 = bits 0-3, pins 7-10 = bits 4-7. Refs J11/J12/J13 (v3.2), J14/J15/J16 (v3.3). No buffers.
* Single-row PMODSIL headers J4, J6, J9, J10 and the management header (J8 v3.2 / J7 v3.3) are DNP.
* Analog: v3.2 has 10 MOhm pull-downs on all analog pins. v3.3 replaces them with TVS diodes and adds an op-amp current source (IREF). Not relevant to our digital bus.

## 6. Flash, PSRAM, crystal, USB, MCU limits

* Flash: Winbond W25Q32JVSSIQ, 4 MB QSPI (U4 v3.2, U7 v3.3). [src: pcb-files ETR/demoboard/v3p2/bom/ttdbv3-populated-bom.csv and v3p3 BOM, retrieved 2026-11-21]
* No PSRAM on either revision. [src: same BOMs and netlists, retrieved 2026-11-21]
* The QMI CS1n-capable GPIOs are 0, 8, 19 and 47. On the board they are ctrl_ena, mng5, ui_in[2] and unconnected. A PSRAM cannot be added without rework (inference). [src: RP2350 datasheet function table; netlists, retrieved 2026-11-21]
* Crystal Y1: 12.000 MHz, FC3BAEBGI12.0-T1, "10ppm stability" (alt ABM8W-12.0000MHZ). [src: v3.2 BOM csv; repos/tt-demo-pcb/rp2350.kicad_sch, retrieved 2026-11-21]
* USB: RP2350 native USB device, 27 Ohm series resistors, TVS diodes D1/D2, CC1/CC2 5.1k to GND (sink), SBU not connected. Shell to GND through DNP 1M/6800 pF. [src: tt-devboard-netlists.txt, retrieved 2026-11-21]
* RP2350: dual Cortex-M33 or Hazard3 at 150 MHz. 520 kB SRAM. Three PIO blocks with four state machines each. [src: RP2350 datasheet sections 1 and 11, retrieved 2026-11-21]

## 7. Breakout boards

### 7.0 How the chip arrives, and custom carrier boards (added 2026-11-21)

* Package: the SKY130 chip is a QFN-64, 9 x 9 mm, 0.5 mm pitch, with a 7.65 x 7.65 mm exposed pad. Not BGA, not DIP. [src: repos/breakout-pcb/breakout-sky-qfn/breakout-sky-qfn.kicad_pcb footprint "QFN-64-1EP_9x9mm_P0.5mm_EP7.65x7.65mm", retrieved 2026-11-21]
* It arrives soldered on the SKY QFN breakout. The breakout carries the 1.8 V core regulator (AP2112K-1.8), decoupling, the mux pull-downs, and two Hirose DF12 receptacles underneath: DF12NB-60DS-0.5V(51) and DF12NB-50DS-0.5V(51). The demoboard has the mating plugs DF12NB(5.0)-60DP-0.5V(51) and DF12NB(5.0)-50DP-0.5V(51) (5.0 mm mated height). [src: same file; repos/tt-demo-pcb schematic, retrieved 2026-11-21]
* TT's stated route for custom boards: design your board to take the breakout ("2 connectors where you get all of the signals of the chip"). A bare chip only by desoldering it from the breakout, or from an "ASIC kit" in the TT shop, not offered for every shuttle. GF180 chips are chip-on-board and cannot be removed; SKY130 QFNs can. [src: https://tinytapeout.com/faq/ and https://www.tinytapeout.com/specs/pcb, retrieved 2026-11-21]
* The DIP breakout (32-pin, with a CH32X035 MCU) exists only for GF180 chip-on-board dies, not for SKY130. [src: repos/breakout-pcb/breakout-dip-gf/README.md, retrieved 2026-11-21]
* Parts for a carrier board: the 60-pin plug is LCSC C2847989 (11 in stock when checked; DigiKey lists a 12-week factory lead time); Hirose rates DF12 at 50 mating cycles. The 50-pin plug's stock was not checked. [src: https://lcsc.com/product-detail/Board-to-Board-and-Backplane-Connector_HRS-Hirose-DF12NB-5-0-60DP-0-5V-51_C2847989.html; https://www.hirose.com/product/p/CL0537-0882-0-51; https://www.digikey.jp/en/products/detail/hirose-electric-co-ltd/DF12NB-5-0-60DP-0-5V-51/13546978, retrieved 2026-11-21]
* Recommendation (inference): a custom JLCPCB board should carry the two DF12 plugs at the demoboard's positions (copy them from repos/tt-demo-pcb) and take the breakout as delivered. Have JLCPCB assemble the 0.5 mm connectors. Do not move the bare QFN: it needs hot-air removal and reflow with a stencil for the exposed pad, risks the only chip, and the new board would have to copy the breakout's regulator and decoupling as well.


* SKY130 QFN breakout: PCB title "QFN Breakout For TinyTapeout Skywater ASICs", rev 3.2, 2025-10-04 (schematic title block says 3.1, 2025-12-02). Chip footprint Openframe_QFN_TT06_REV2 (QFN64). [src: repos/breakout-pcb/breakout-sky-qfn/, retrieved 2026-11-21]
* On the SKY breakout the only parts on IO nets are test pads TP1 (rst_n), TP2 (clk), TP3 (uio[1]), TP4 (ui_in[0]). No series resistors. No pulls. [src: tt-devboard-netlists.txt section 3, retrieved 2026-11-21]
* SKY breakout mux pulls: ctrl_ena 1k to GND (R9). ctrl_sel_rst_n 1k to GND (R10, through R5 0 Ohm). ctrl_sel_inc has none. Chip resetb is not connected (R6 DNP). 24LC02 EEPROM DNP. [src: same, retrieved 2026-11-21]
* GF180 COB breakout rev 0.2 (2026-10-28): same DF12 pin mapping. ctrl_ena 1k to GND (R11), ctrl_sel_rst_n 1k to GND (R10). Test pads on clk, rst_n, uio[0]. [src: tt-devboard-netlists.txt section 4, retrieved 2026-11-21]
* Purpose of the strong breakout pull-downs vs 10k demoboard pull-ups: "On startup, read the ctrl en and reset, if they are low, we have an ASIC carrier." [src: repos/tt-demo-pcb/doc/tt-etr-dbv3.pdf "Misc Support", retrieved 2026-11-21]
* Consequence (inference): whenever the RP is in reset, in BOOTSEL, or leaves GPIO0/1 undriven, ctrl_ena and ctrl_sel_rst_n sit at about 0.3 V (computed: 3.3 V x 1k / 11k). The mux is reset and disabled.

## 8. Stock firmware (TT MicroPython SDK)

Repo: repos/tt-micropython-firmware, HEAD 15ba5ced (2026-11-12). Latest release v3.1.1 (2026-09-08), asset tt-demo-rp2350-v3.1.1.uf2, 8,388,608 bytes. The HEAD commit adds ttgf0p3/ttgf26a/ttgf26b shuttle files that are not in a release yet. [src: https://github.com/TinyTapeout/tt-micropython-firmware/releases, retrieved 2026-11-21]

* Since 2026-01-27 the SDK supports only RP2350 / ETR boards. [src: commit 37cf3e87 "Exclusive RP2350/ETR DB support", retrieved 2026-11-21]
* GPIO map class GPIOMapTTDBv3 matches section 3: PROJECT_nRST=14, MANUAL_PROJCLK=15, RP_PROJCLK=16, CTRL_SEL_ENA=0, CTRL_SEL_nRST=1, CTRL_SEL_INC=2, UI_IN0=17, UIO0=25, UO_OUT0=33, RP_LED=11, ANALOG_CURRENT_SOURCE=12, MNG00-07=3-10. [src: src/ttboard/pins/gpio_map_dbv3.py, retrieved 2026-11-21]
* Default pull for every SDK-managed pin is PULL_DOWN, except GPIO12 and GPIO15 (PULL_UP). [src: same file, default_pull, retrieved 2026-11-21]

### 8.1 Boot sequence

* main.py prints the banner, runs DemoboardDetect.probe(), then DemoBoard.get(). [src: src/main.py, retrieved 2026-11-21]
* probe(): sets GPIO0-40 as inputs with pull-down. Reads GPIO10 (mng7): high = FPGA breakout. Reads ctrl_ena and ctrl_sel_rst_n: both low = ASIC carrier. Reads GPIO15 with pull-up: 0 = v3.3, else v3.2. [src: src/ttboard/boot/demoboard_detect.py, retrieved 2026-11-21]
* The SDK then reads the chip ROM at mux address 0 to identify the shuttle, and loads the default project from config.ini (tt_um_factory_test). [src: src/ttboard/boot/rom.py; src/config.ini, retrieved 2026-11-21]
* config.ini defaults: mode = ASIC_RP_CONTROL, rp_clock_frequency = 125e6, start_in_reset = no. [src: src/config.ini, retrieved 2026-11-21]

### 8.2 Project selection (mux)

* Sequence in ProjectMux.reset() and reset_and_clock_mux(count): ctrl_sel_inc=0, ctrl_sel_rst_n=0, ctrl_ena=0. Wait 10 ms. ctrl_sel_rst_n=1. Wait 10 ms. Then count times: ctrl_sel_inc=1, wait 1 ms, ctrl_sel_inc=0, wait 1 ms. Then ctrl_ena=1. [src: src/ttboard/project_mux.py lines 503-578, retrieved 2026-11-21]
* count is the project "address" from the shuttle index JSON (shuttles/<shuttle>.json, built from https://index.tinytapeout.com/<chip>.json). [src: src/ttboard/project_mux.py, project_design.py, retrieved 2026-11-21]
* enable() first calls disable(): uio pins become RP inputs, the mux is reset to 0, ctrl_ena stays 0. [src: same, retrieved 2026-11-21]
* Subtile designs: the SDK drives uio[0..n-1] as a subtile address. Our design must be a normal project or it loses uio pins. [src: same, enable(), retrieved 2026-11-21]
* Mux hardware: a 10-bit ripple counter. sel_inc clocks the first flip-flop. sel_rst_n resets all. ena is buffered to the mux units. [src: https://github.com/TinyTapeout/tt-multiplexer/blob/main/docs/INFO.md, retrieved 2026-11-21]
* Inactive designs get clk, rst_n, ui_in and uio_in tied to 0. Their uo_out, uio_oe and uio_out are disconnected by tristate buffers. [src: same, retrieved 2026-11-21]
* Faster timing also works in practice: Mike Bell's C example uses 100 us reset low, 100 us high, 10 us inc pulses. [src: repos/tt-rp2040/tt_setup.c, retrieved 2026-11-21]

### 8.3 Project clock

* clock_project_PWM(f): for f >= 3 Hz it uses machine.PWM on GPIO16 (PWM0 A), 50% duty. It first picks an RP system clock so that sys / divisor hits f, divisor an even integer >= 2. [src: src/ttboard/demoboard.py clock_project_PWM and _get_best_rp2040_freq, retrieved 2026-11-21]
* Max f is sys / 2. Default sys ceiling is 133 MHz. If 2f > 133 MHz and 2f <= 250 MHz the SDK raises sys to 2f, so up to 125 MHz with sys at 250 MHz. That exceeds the 150 MHz RP2350 rating and the 66 MHz chip input rating. [src: same; RP2350 datasheet; https://tinytapeout.com/specs/gpio/, retrieved 2026-11-21]
* If the achieved frequency differs by more than 1 Hz the SDK logs a warning with the actual value. Accuracy otherwise follows the 12 MHz crystal, 10 ppm stability (inference from integer division of the PLL clock).
* For f < 3 Hz it uses a PIO toggler: sys set to 100 MHz, state machine at 2 kHz with a delay loop. [src: src/ttboard/util/platform/rp2350.py PIOClock, retrieved 2026-11-21]
* clock_project_once() toggles GPIO16 twice. clock_project_stop() stops PWM or PIO. [src: src/ttboard/demoboard.py, retrieved 2026-11-21]
* The TT clock spec says 1 Hz to 66.5 MHz and up to 10 ns insertion delay from pad to project clock. That page still describes the RP2040. [src: https://tinytapeout.com/specs/clock/, retrieved 2026-11-21]

### 8.4 Reset (rst_n)

* reset_project(True): GPIO14 output, drive 0. reset_project(False): write 1, then switch GPIO14 to input and rely on the board pull-up (v3.2) or inverter (v3.3). [src: src/ttboard/demoboard.py reset_project, retrieved 2026-11-21]
* On first enable of a design it pulses reset for 2 ms. start_in_reset in config.ini keeps it in reset. [src: same, _first_encouter_reset and apply_user_config, retrieved 2026-11-21]
* v3.2 detail: with GPIO14 as input and the SDK pull-down on, rst_n idles at about 2.5-2.7 V (computed: 11k up vs 36-113k down). That is still a logic high.

### 8.5 Driving ui_in and reading uo_out / uio

* API: tt.ui_in.value = byte, tt.ui_in[n] = bit, tt.uo_out.value, tt.uio_oe_pico.value (bit 1 = RP drives that uio), tt.uio_in.value (RP writes into the chip's uio_in), tt.uio_out.value (RP reads the chip's uio_out). [src: src/main.py banner text; src/ttboard/demoboard.py, retrieved 2026-11-21]
* Per-project config.ini keys: clock_frequency, rp_clock_frequency, ui_in, uio_oe_pico, uio_in, mode, start_in_reset. [src: src/config.ini; config.md, retrieved 2026-11-21]
* Byte access uses direct SIO register writes. ui_in is GPIO_OUT bits 17-24. uio[0..6] are GPIO_IN bits 25-31 and uio[7] is GPIO_HI_IN bit 0. uo_out is GPIO_HI_IN bits 1-8. [src: src/ttboard/util/platform/rp2350.py, retrieved 2026-11-21]
* Modes: SAFE (all inputs), ASIC_RP_CONTROL (RP drives ui_in, clk), ASIC_MANUAL_INPUTS (ui_in from DIP/PMOD, clk and rst_n inputs). [src: src/config.ini; src/ttboard/pins/pins.py, retrieved 2026-11-21]
* Always-output pins in the SDK: GPIO11, mng00, mng02, mng03, ctrl_sel_inc, ctrl_ena, ctrl_sel_rst_n, GPIO12. [src: gpio_map_dbv3.py always_outputs, retrieved 2026-11-21]

## 9. Custom C (pico-sdk) firmware

* Yes, the RP2350 runs any UF2. The official install steps: hold the BOOT button, connect USB, release, the RP2350 drive appears, copy the UF2, the drive disappears when done. [src: repos/tt-micropython-firmware/README.md "Installation", retrieved 2026-11-21]
* The BOOT button pulls QSPI_SS low, which is the RP2350 bootrom BOOTSEL condition. [src: repos/tt-demo-pcb/doc/tt-etr-dbv3.pdf "RP Boot Mode"; RP2350 datasheet section 5.2.8, retrieved 2026-11-21]
* There is no reset button (RUN header DNP). Replug USB to reset. [src: tt-devboard-netlists.txt net /RUN, retrieved 2026-11-21]
* Restore stock: same steps with tt-demo-rp2350-v3.1.1.uf2 from the releases page. A firmware upgrade "erases all files on the board", including config.ini. [src: README.md; https://tinytapeout.com/guides/get-started-demoboard-etr/, retrieved 2026-11-21]
* The stock UF2 is 8,388,608 bytes. At 256 payload bytes per 512-byte UF2 block that is the full 4 MB flash (computed). Restoring overwrites any disk image we keep in flash.
* No pico-sdk board header exists for the TT board. pico2.h sets PICO_RP2350A 1 (30 GPIOs only). pico2.h and pimoroni_pga2350.h both put the default UART on GPIO0/1 (ctrl_ena, ctrl_sel_rst_n) and the default LED on GPIO25 (uio[0]). [src: https://github.com/raspberrypi/pico-sdk/tree/master/src/boards/include/boards (commit 079c6f39), retrieved 2026-11-21]
* So a custom build needs its own board header: PICO_RP2350A 0, PICO_FLASH_SIZE_BYTES 4 MB, PICO_DEFAULT_LED_PIN 11. Disable UART stdio, use USB stdio. The community header repos/tt_terminal_explorer/firmware/boards/tt_dbv3.h does exactly this. [src: that file, retrieved 2026-11-21]

### 9.1 What custom firmware must replicate (inference from sections 2-8)

* Keep GPIO0-2 low until ready. Select the project: ctrl_ena=0, ctrl_sel_rst_n pulse low, N pulses on ctrl_sel_inc, ctrl_ena=1. Hold GPIO2 steady low after that. Any edge on it moves the mux address.
* Drive rst_n (GPIO14) low before ctrl_ena goes high. Start the clock. Release rst_n after some clock cycles.
* Generate the clock on GPIO16 (PWM slice 0 A or PIO side-set).
* Leave GPIO15 as an input (v3.3 inverter output). Leave GPIO12 high or as input (v3.3 current source).
* Drive ui_in at safe idle values. Keep uio as inputs until the protocol says otherwise.
* Power-up order: nothing to sequence. Chip and RP power up together. Until firmware runs, the breakout pull-downs hold the mux disabled and in reset. The design then sees rst_n=0 and clk=0 internally (section 8.2).
* Use at least 4 mA drive on GPIO0, GPIO1, GPIO14. Each sources or sinks about 3 mA into a 1k pull (computed). 8 mA gives margin (suggestion).
* v3.3: to hold reset, GPIO14 must sink about 3.3 mA against U3 through 1k (computed). v3.2: rst_n has 100 nF, so edges take tens of microseconds (computed, rough: about 170 Ohm x 100 nF = 17 us time constant).

### 9.2 Examples found

* MichaelBell/tt-rp2040 (cloned): RP2040 C SDK example for TT04/05. Has tt_select_design() and clock helpers. Its pin map is for a "PGA2350-TT06 with broken TT05 carrier converter", not the v3 board. [src: repos/tt-rp2040/tt_pins.h, README.md, retrieved 2026-11-21]
* MichaelBell/spi-ram-emu (cloned by a parallel task): "Simulated SPI RAM on RP2040". SPI mode only, 64 kB region at 0x20030000, READ up to sys/10, WRITE up to sys/6, uses core1 and most of both RP2040 PIOs. Last code commit 2023-07-24. [src: repos/spi-ram-emu/README.md, retrieved 2026-11-21]
* spi-ram-emu was used with TT04 nanoV ("use the RP2040 emulated SPI RAM"). [src: repos/tt04-nanoV/info.yaml line 64, retrieved 2026-11-21]
* TinyQV does not use RP emulation. TT's RISC-V guide says to plug a QSPI Pmod (16 MiB flash + 2 x 8 MiB PSRAM) into the Bidir (uio) port. [src: https://github.com/TinyTapeout/tt-riscv/blob/main/guides/board-setup.md, retrieved 2026-11-21]
* No RP2350 RAM emulation repo by Michael Bell or TinyTapeout was found. I listed all MichaelBell repos and searched the TinyTapeout org. [src: https://github.com/MichaelBell?tab=repositories ; https://github.com/orgs/TinyTapeout/repositories, retrieved 2026-11-21]
* Community, not official: map588/tt_terminal_explorer (cloned). RP2350 C firmware for the v3 board. Board header, mux select copied from the SDK timings, PIO clock generator. 3 stars, last commit 2026-08-05. Not reviewed in depth. [src: repos/tt_terminal_explorer/firmware/, retrieved 2026-11-21]

## 10. PIO across the GPIO31/32 boundary

* GPIOBASE register (PIO offset 0x168): "Relocate GPIO 0 (from PIO's point of view) in the system GPIO numbering, to access more than 32 GPIOs from PIO. Only the values 0 and 16 are supported (only bit 4 is writable)." [src: RP2350 datasheet, PIO GPIOBASE register, Table 1070, retrieved 2026-11-21]
* "Each PIO block is still limited to 32 GPIOs at a time, but GPIOBASE selects which 32." GPIOBASE is per PIO block, so all four state machines of that block share it. [src: RP2350 datasheet 11.1.1, retrieved 2026-11-21]
* With base 16, PIO pin n is GPIO 16+n, n = 0..31, so GPIO16-47. Our pins: clk GPIO16 = PIO pin 0, ACK GPIO17 = pin 1, IRQ GPIO18 = pin 2, uio GPIO25-32 = pins 9-16, REQ GPIO33 = pin 17. The 31/32 split does not exist inside PIO. [src: same register text; inference for the pin arithmetic]
* The 31/32 split exists only in SIO: GPIO_IN covers 0-31, GPIO_HI_IN covers 32-47. CPU reads of uio need two registers. PIO reads do not. [src: repos/tt-micropython-firmware/src/ttboard/util/platform/rp2350.py register notes, retrieved 2026-11-21]
* IN: "the LSB is the GPIO selected by PINCTRL_IN_BASE, and successively more-significant bits come from successively higher-numbered GPIOs, wrapping after 31." Our window (PIO pins 9-17) does not wrap. SHIFTCTRL.IN_COUNT (new on RP2350) masks unused IN bits to zero. [src: RP2350 datasheet 11.5.6.2 and 11.1.1, retrieved 2026-11-21]
* JMP PIN: EXECCTRL_JMP_PIN selects "one out of the maximum of 32 GPIO inputs visible to a state machine". Use GPIO33 (PIO pin 17). [src: RP2350 datasheet, JMP instruction, retrieved 2026-11-21]
* WAIT GPIO uses an absolute 5-bit index. WAIT PIN adds Index to PINCTRL_IN_BASE modulo 32. WAIT JMPPIN (new) uses JMP_PIN plus 0-3. [src: RP2350 datasheet, WAIT instruction, retrieved 2026-11-21]
* pico-sdk pio_add_program() flips bit 4 of every WAIT GPIO index when the block's base is 16. So "wait 1 gpio 33" in pioasm lands on the right pin. [src: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_pio/pio.c add_program_at_offset, retrieved 2026-11-21]
* OUT, SET, side-set, IN base and JMP PIN are 5-bit fields relative to GPIOBASE. The SDK sm_config_set_*_pin* functions take real GPIO numbers 0-47 and pio_sm_set_config() converts them. It returns PICO_ERROR_BAD_ALIGNMENT if a config mixes pins outside the block's window. [src: pico-sdk hardware/pio.h comments and pio_sm_set_config, retrieved 2026-11-21]
* SDK calls: pio_set_gpio_base(pio, 16) before loading any program into that block (it returns PICO_ERROR_INVALID_STATE if instructions are already loaded). Or pio_claim_free_sm_and_add_program_for_gpio_range(&prog, &pio, &sm, &offset, 16, 18, true) to find a block and set the base for GPIO16-33. Then pio_gpio_init(pio, pin) per pin. [src: pico-sdk pio.c lines 84-104 and 454-500, retrieved 2026-11-21]
* Needs PICO_PIO_USE_GPIO_BASE, which defaults on when NUM_BANK0_GPIOS > 32, so PICO_RP2350A must be 0. Latest pico-sdk release 2.3.1 (2026-09-04). [src: pico-sdk pio.h line 150; https://github.com/raspberrypi/pico-sdk/releases, retrieved 2026-11-21]
* Uncertain: the datasheet does not list every instruction under GPIOBASE = 16 one by one. The per-instruction behaviour above combines the register text, the IN-mapping text and the SDK code. A loopback test on the first board would confirm it.
* Each GPIO input has a 2-flipflop synchroniser (2 clk_sys cycles latency). INPUT_SYNC_BYPASS can bypass it per GPIO. Keep it on for the async REQ. [src: RP2350 datasheet 11.5.6.3, retrieved 2026-11-21]

## 11. What on the board can fight the bus

* DIP switches on ui_in[0] (ACK), ui_in[1] (IRQ), ui_in[2], ui_in[4] (debug) and ui_in[3] (UART): ON = 1k to 3V3. Keep SW1 positions 1-5 OFF, ideally all eight. (section 5)
* Input PMOD, Bidir PMOD and Output PMOD headers: keep them empty. A QSPI Pmod in the Bidir port would sit directly on our data bus. (section 5)
* 7-segment on uo_out: REQ (segment A), debug uo_out[1] (segment B) and chip UART TX uo_out[4] (segment E) each source about 1.4-2.5 mA into an LED. It is a resistive load, not a fight. The chip pad is rated 4 mA. Cut JP1/JP2/JP5 (v3.2) or JP2/JP3/JP6 (v3.3) if the margin matters. (section 5)
* uio: no pulls anywhere. When neither side drives (bus turnaround, project disabled, chip in reset) the lines float. On RP2350 A2 (erratum E9) a floating input can sit at about 2.2 V and the internal pull-down cannot pull it low. Pull-ups still work. [src: RP2350 datasheet D.5.1, retrieved 2026-11-21]
* E9 details: affects A2 only, fixed in A3. A4 is the production version. It needs IE=1, output disabled and the pad in the undefined region. Pull-down works right after reset because IE starts at 0. PIO cannot toggle pad controls. Workaround: external pull-down of 8.2 kOhm or less, or use pull-ups. [src: RP2350 datasheet D.5.1, C.2.1, Table 7, retrieved 2026-11-21]
* Stepping on the board: unknown. BOM MPN "RP2350B", LCSC C42415655, no stepping. The LCSC page shows no stepping either. Package marking shows "A2", "A3" or "A4". Bootrom byte at 0x00000013 reads 2, 3 or 4. [src: v3.2/v3.3 BOMs; https://www.lcsc.com/product-detail/C42415655.html; RP2350 datasheet 14.4 and 5.4, retrieved 2026-11-21]
* Suggested mitigation for E9 (suggestion): keep one side always driving uio (bus parking), or enable RP pull-ups on uio. Avoid relying on RP pull-downs.
* REQ and uio when the project is not enabled: the mux disconnects them with tristate buffers. The pad state then is not documented. Treat REQ as invalid until ctrl_ena=1 and rst_n has been released. [src: tt-multiplexer docs/INFO.md; inference]
* Stock firmware sets pull-downs on all ui/uio/uo pins. That is fine on A4 and weak on A2. (section 8)
* clk: v3.2 has 47 Ohm in series plus a 10k branch to the manual-clock network. Pressing SW3 injects 3V3 through 10k. The RP wins. v3.3 has no series resistor. Expect more ringing at high clock rates (inference). Choose drive strength and slew on GPIO16 with that in mind.
* rst_n: v3.2 has 100 nF on the net, so rst_n cannot carry fast signals. v3.3 has U3 driving it high through 1k. On both, pressing SW4 while the RP drives rst_n high does nothing useful (computed: 3.3 mA through 1k, RP holds the line).
* ctrl lines: 10k or 5.1k pull-ups on the demoboard, 1k pull-downs on ctrl_ena and ctrl_sel_rst_n on the breakout. Driving them costs about 3 mA each (computed). The RP output must stay above the chip input threshold. VOH min 2.62 V at rated drive.
* GPIO15 (v3.3): driven by U2. Never configure it as an output.
* Subtile mode in stock firmware drives uio[0..n-1]. Make sure the design is registered as a normal project. (section 8.2)
* Nothing else loads the bus nets: breakout test pads only. (section 7)

## 12. Mismatches with the earlier notes

* "rev 3.2": v3.2 exists, but v3.3 (2026-06-23) is also fabricated and supported. The board we get may be v3.3.
* "47 ohm on the clock (GPIO16)": true on v3.2 only. v3.3 connects GPIO16 straight to the chip clk. v3.2 also has a 10k branch from the manual-clock button on the clk net.
* "reset is GPIO14": correct. Missing detail: v3.2 has 100 nF and an 11k pull-up on rst_n. v3.3 drives rst_n from a Schmitt inverter through 1k.
* "1k behind each DIP switch": correct. The 1k runs from +3V3 to the switch. The switch closes onto the ui_in net.
* "510 ohm on 7-seg jumpers": correct. The jumpers are solder jumpers, bridged by default. The 510 Ohm sits after the jumper.
* "150 MHz": that is the RP2350 rating. Stock config runs the RP at 125 MHz and the SDK may raise it to 250 MHz for fast project clocks.
* Confirmed unchanged: RP2350B, GPIO17-24 / 25-32 / 33-40 map, plain copper with no series resistors on ui/uio/uo, one W25Q32JV 4 MB flash, no PSRAM.
* Not in the notes: mux control on GPIO0-2, v3.3 uses GPIO15 (manual clock input) and GPIO12 (current source).

## 13. Open questions and uncertain items

* Which demoboard revision ships with our chip. Not stated anywhere.
* RP2350 stepping on the board (E9 risk on A2).
* Chip pad output state on uo_out and uio while no project is enabled.
* Chip-side input thresholds of the SKY130 pad at 3.3 V are not on the TT GPIO page.
* The v3.3 inverter part: schematic NC7SP14P5X, BOM NC7SV14P5X. Function is the same.
* The store links "breakout-skyqfn-v3p2". The breakout-pcb repo has no such tag. The pcb-files archive folder ETR/breakout-skyqfn-v3p2 matches the PCB rev 3.2 used here.
* PIO behaviour per instruction with GPIOBASE = 16 (section 10, last bullets).

## 14. Sources (all retrieved 2026-11-21)

* https://tinytapeout.com/guides/get-started-demoboard-etr/
* https://tinytapeout.com/guides/get-started-demoboard/ (TT04-TT07, RP2040, stale)
* https://tinytapeout.com/specs/pcb-etr/
* https://tinytapeout.com/specs/pcb/ (RP2040 era, stale)
* https://tinytapeout.com/specs/gpio/
* https://tinytapeout.com/specs/clock/
* https://tinytapeout.com/specs/pinouts/
* https://github.com/TinyTapeout/tt-demo-pcb (HEAD bbf3dd1f; v3.2 files from commit 0277545d)
* https://github.com/TinyTapeout/pcb-files (ETR/demoboard/v3p2, v3p3, breakout folders)
* https://github.com/TinyTapeout/breakout-pcb (HEAD 91a2bf18)
* https://github.com/TinyTapeout/tt-micropython-firmware (HEAD 15ba5ced, release v3.1.1)
* https://github.com/TinyTapeout/tt-multiplexer/blob/main/docs/INFO.md
* https://github.com/TinyTapeout/tt-riscv/blob/main/guides/board-setup.md
* https://store.tinytapeout.com/ (kit pages listed in section 1)
* https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf (redirects to https://pip-assets.raspberrypi.com/categories/1214-rp2350/documents/RP-008373-DS-4-rp2350-datasheet.pdf; release 9, build 19/11/2026, f6dffdf503a8)
* https://github.com/raspberrypi/pico-sdk (master 079c6f39; release 2.3.1)
* https://github.com/MichaelBell/tt-rp2040 ; https://github.com/MichaelBell/spi-ram-emu
* https://github.com/map588/tt_terminal_explorer (community)
* https://www.lcsc.com/product-detail/C42415655.html

## 15. Repos and files used

* repos/tt-demo-pcb: git clone --depth 1, HEAD bbf3dd1f (2026-08-29), 128 MB.
* repos/breakout-pcb: git clone --depth 1, HEAD 91a2bf18 (2026-10-29), 135 MB.
* repos/tt-rp2040: git clone --depth 1, HEAD 8779b9f3 (2025-02-01), 164 KB.
* repos/tt_terminal_explorer: git clone --depth 1, HEAD 56c0d33e (2026-08-05), 776 KB. Community code.
* repos/tt-micropython-firmware (HEAD 15ba5ced, 8.7 MB) and repos/spi-ram-emu (HEAD 7a46f8f5, 240 KB) were cloned by a parallel task in the same folder. Used read-only.
* tt-devboard-netlists.txt: pad-level netlists of demoboard v3.2, v3.3, SKY130 breakout and GF180 breakout.
* No code from any cloned repo was run.

# Raspberry Pi notes for asc (RP2040, RP2350, Pico, Pico 2)

Scope: item 3 of the KB brief. All facts retrieved 2026-11-21. Datasheet page numbers refer to the PDFs in `rp/`. Line numbers refer to the text conversions in `rp/txt/` (made with pdftotext -layout).

---------------------------------------------------------------------

## 1. Files downloaded (official sources only)

All requests to datasheets.raspberrypi.com redirected to pip-assets.raspberrypi.com, the Raspberry Pi product information portal.

* rp/rp2040-datasheet.pdf - 6,780,766 bytes, 615 pages, Release 13, build date 19/11/2026. From https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf (served as RP-008371-DS-2). [retrieved 2026-11-21]
* rp/rp2350-datasheet.pdf - 11,097,563 bytes, 1371 pages, Release 9, build date 19/11/2026. From https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf (served as RP-008373-DS-4). [retrieved 2026-11-21]
* rp/pico-datasheet.pdf - 18,120,505 bytes, 31 pages, Release 22, build date 19/11/2026. From https://datasheets.raspberrypi.com/pico/pico-datasheet.pdf (served as RP-008307-DS-3). [retrieved 2026-11-21]
* rp/pico-2-datasheet.pdf - 3,016,817 bytes, 24 pages, Release 5, build date 03/07/2026. From https://datasheets.raspberrypi.com/pico/pico-2-datasheet.pdf (served as RP-008299-DS-3). [retrieved 2026-11-21]
* rp/RP235x-A4-stepping-PCN.pdf - 230,511 bytes, 7 pages, "PCN 28 - A4 stepping", Release 2, 29 Jul 2025. From https://pip.raspberrypi.com/categories/1263-pcn/documents/RP-008771-CC/RP235x-A4-stepping-PCN.pdf. [retrieved 2026-11-21]
* rp/txt/*.txt - text conversions of the five PDFs, for grep. Total about 6.4 MB.
* RP2350 errata: no separate errata document is published. The errata are Appendix D of the RP2350 datasheet (pages 1351 onward). The Raspberry Pi silicon documentation page links only the two datasheets and the portal. [https://www.raspberrypi.com/documentation/microcontrollers/silicon.html via WebFetch (curl got HTTP 403), retrieved 2026-11-21]

---------------------------------------------------------------------

## 2. Which RP2 is on the TT board, and the pin map

* The latest TT demo boards (ETR, TT DB v3) use an RP2350B (QFN-80, 48 GPIO). All IO expects 3.3 V. [https://tinytapeout.com/guides/get-started-demoboard-etr/, retrieved 2026-11-21]
* RP2350A has 30 GPIO (QFN-60). RP2350B has 48 GPIO (QFN-80). GPIO 30-47 exist only on QFN-80. [rp2350-datasheet.pdf Table 1 page 13 and page 20, txt lines 826-830 and 1337, retrieved 2026-11-21]
* Pico 2 uses RP2350A, so it cannot reach GPIO 30-47. [pico-2-datasheet.pdf page 3, retrieved 2026-11-21]
* TT firmware pin map for the production ETR board (class GPIOMapTTDBv3), RP2350B GPIO numbers: [repos/tt-micropython-firmware/src/ttboard/pins/gpio_map_dbv3.py lines 90-140, commit 15ba5ce, retrieved 2026-11-21]
  * ctrl_ena GPIO0, ctrl_sel_rst_n GPIO1, ctrl_sel_inc GPIO2
  * project rst_n GPIO14, manual clock button GPIO15, project clk GPIO16
  * ui_in[0..7] = GPIO17..24
  * uio[0..7] = GPIO25..32
  * uo_out[0..7] = GPIO33..40
* For asc this means: clk = GPIO16, ACK on ui_in[0] = GPIO17, data uio[7:0] = GPIO25-32, REQ on uo_out[0] = GPIO33, rst_n = GPIO14. [derived from the map above]
* uio straddles the 32-bit SIO boundary: uio7 = GPIO32 is in the HI registers. The TT firmware reads it with two registers. [repos/tt-micropython-firmware/src/ttboard/util/platform/rp2350.py, retrieved 2026-11-21]
* Older DB v3 "Alpha" map is different (clk GPIO21, rst GPIO20, uo_out GPIO30-37). The firmware uses the production map by default. Confirm your board. [same gpio_map_dbv3.py; boot/demoboard_detect.py lines 150-182, retrieved 2026-11-21]
* Caution: the TT firmware clock_project_PWM() changes the RP system clock to hit a requested project clock. It allows up to 250 MHz when the request is above 66.5 MHz. Any PIO timing you set up shifts with it. [repos/tt-micropython-firmware/src/ttboard/demoboard.py lines 340-392 and 449-460, retrieved 2026-11-21]

---------------------------------------------------------------------

## 3. PIO

### 3.1 GPIOBASE (RP2350 only) - reaching GPIO 16-47

* Each PIO block still sees 32 GPIOs at a time. GPIOBASE selects which 32. [rp2350-datasheet.pdf section 11.1.1, page 792, txt line 50782, retrieved 2026-11-21]
* Register GPIOBASE at PIO offset 0x168. Only values 0 and 16 are supported. Only bit 4 is writable. Reset 0. [rp2350-datasheet.pdf page 865, txt lines 55969-55985, retrieved 2026-11-21]
* With GPIOBASE = 16, PIO pin 0 is GPIO16, so the block reaches GPIO16-47. [same]
* RP2350 has 12 PIO state machines (3 blocks of 4). GPIOBASE is per block, so one block can use base 16 and another base 0. [rp2350-datasheet.pdf section 1 feature list, page 13, txt line 812; SDK pio.h, retrieved 2026-11-21]
* pico-sdk: pio_set_gpio_base(pio, 0 or 16). It only changes the register and does not protect running state machines. [pico-sdk 2.3.1 src/rp2_common/hardware_pio/include/hardware/pio.h lines 984-1000, https://raw.githubusercontent.com/raspberrypi/pico-sdk/2.3.1/src/rp2_common/hardware_pio/include/hardware/pio.h, retrieved 2026-11-21]
* pico-sdk: on RP2350B the pio_sm_ functions take real GPIO numbers and subtract the GPIO base internally. PICO_PIO_USE_GPIO_BASE defaults to 1 when the device has more than 32 GPIOs. Programs record which 16-pin ranges they use and are rejected if the base does not fit. [same pio.h lines 66-90, 150-155, 982, retrieved 2026-11-21]
* For the ETR map: with base 16, clk = PIO pin 0, ui_in = pins 1-8, uio = pins 9-16, uo_out = pins 17-24. rst_n (GPIO14) and the mux control pins (GPIO0-2) are outside that window. Drive them from the CPU or from a second PIO block with base 0. [derived from section 2 map]
* The RP2350 HSTX coupled mode note confirms the remap: with GPIOBASE 16, PIO outputs that would appear on GPIOs 28-35 are used. [rp2350-datasheet.pdf txt lines 72781-72782, retrieved 2026-11-21]

### 3.2 Input synchronizer and how to bypass it

* Every GPIO input to PIO goes through a 2-flipflop synchronizer. It adds two cycles of latency. It is needed for asynchronous inputs such as UART RX. [rp2350-datasheet.pdf section 11.5.6.3, page 824, txt lines 53032-53041; rp2040-datasheet.pdf page 330, retrieved 2026-11-21]
* Bypass per GPIO by setting the bit in INPUT_SYNC_BYPASS (PIO offset 0x038, reset 0x00000000, 1 = bypassed). The datasheet says only do it when the state machine never samples at an unsafe moment, which is generally only possible for synchronous interfaces such as SPI. Sampling a metastable input can make the state machine misbehave. [rp2350-datasheet.pdf page 854 Table 1046 and page 824; rp2040-datasheet.pdf page 359, retrieved 2026-11-21]
* With GPIOBASE = 16, bit n of INPUT_SYNC_BYPASS is relative to the base. pico-sdk pio_set_input_sync_bypass_with_mask() shifts the mask right by the gpio base before writing. [pico-sdk 2.3.1 src/rp2_common/hardware_pio/pio.c lines 223 and 345-370, retrieved 2026-11-21] The datasheet itself does not state the bit mapping under GPIOBASE. This rests on the SDK code.
* Latency arithmetic: two sys clocks at 150 MHz is 13.3 ns. [inference]
* For asc: REQ (uo_out[0]) is asynchronous to the RP2350, so keep its synchronizer. Data on uio is sampled only after REQ is seen, so it is stable and gains nothing from a bypass. TT measured under 2 ns skew between IO pins on the chip side. [inference; skew from https://tinytapeout.com/specs/gpio/, retrieved 2026-11-21]

### 3.3 Maximum PIO clock and clock divider

* PIO runs from the system clock. A divider of 1 runs the state machine every system clock. [rp2350-datasheet.pdf section 11.5.5, page 821, txt lines 52858-52880, retrieved 2026-11-21]
* Divider: 16-bit integer plus 8-bit fraction, first-order delta-sigma. Range 1 to 65536 in steps of 1/256. [same; rp2040-datasheet.pdf page 327, retrieved 2026-11-21]
* So the maximum PIO clock equals the maximum clk_sys: 150 MHz on RP2350, 133 MHz on RP2040 (200 MHz with DVDD at 1.15 V). See section 5. [derived]
* A state machine can change an output every system clock, so the fastest PIO square wave is clk_sys/2. [inference from section 11.5.6 page 822: each state machine can write GPIOs on every system clock cycle]
* Fractional division adds jitter (delta-sigma). Use an integer divider for clocks that feed the TT chip. [inference from the divider description]
* No PIO erratum exists in RP2350 datasheet Appendix D. The errata sections are ACCESSCTRL, Bootrom, Bus Fabric, DMA, GPIO, Hazard3, OTP, RCP, SIO, XIP, USB. [rp2350-datasheet.pdf Appendix D, retrieved 2026-11-21]

---------------------------------------------------------------------

## 4. GPIO pads: drive strength, slew, reset state

RP2350:
* Drive strength 2, 4, 8 or 12 mA. Slew slow or fast. Schmitt trigger selectable. Pull-up or pull-down. Input buffer can be disabled. Both pulls set together gives bus-keeper mode. [rp2350-datasheet.pdf section 9.6 and 9.6.1, page 543, retrieved 2026-11-21]
* Pad register PADS_BANK0 GPIOn: bit 8 ISO (reset 1), bit 7 OD (0), bit 6 IE (reset 0), bits 5:4 DRIVE (reset 1 = 4 mA), bit 3 PUE (0), bit 2 PDE (reset 1), bit 1 SCHMITT (reset 1), bit 0 SLEWFAST (reset 0 = slow). [rp2350-datasheet.pdf page 705 Table 885, retrieved 2026-11-21]
* At power-up Bank 0 pads are high-Z, input buffer off, pulled low, and isolation latched. Software must set IE = 1 and clear ISO before use. gpio_set_function() in the SDK does both. This differs from RP2040. [rp2350-datasheet.pdf section 9.3 Reset state, Important box, page 539, txt lines 35868-35885, retrieved 2026-11-21]
* Electrical at IOVDD 3.3 V: VIH min 2.0 V, VIL max 0.8 V, hysteresis min 0.2 V, VOH min 2.62 V and VOL max 0.5 V at the selected drive current. Pull-up 32-86 kOhm, pull-down 36-113 kOhm. Total GPIO IOVDD current max 100 mA. [rp2350-datasheet.pdf section 14.9.4 Table 1683, pages 1336-1337, retrieved 2026-11-21]
* No output rise/fall time or maximum toggle frequency is given for Bank 0 GPIO in the datasheet tables I checked. [searched rp2350-datasheet.txt, retrieved 2026-11-21]
* GPIOs are 5 V tolerant when powered and 3.3 V failsafe when unpowered. [rp2350-datasheet.pdf section 1, page 13, txt line 802, retrieved 2026-11-21]

RP2040:
* Same DRIVE and SLEWFAST encoding. Pad reset: IE 1, DRIVE 4 mA, PDE 1, SCHMITT 1, SLEWFAST 0. No ISO bit. [rp2040-datasheet.pdf page 292 Table 346, retrieved 2026-11-21]

For asc (inference, not tested): the defaults are 4 mA and slow slew. For 25 MHz and faster strobes, 8 mA with fast slew gives sharper edges, but sharper edges ring more on jumper wires. Measure before changing. KianV TT05 needed a 22 pF capacitor on a fast clock line (see proven-projects.md 1.2).

---------------------------------------------------------------------

## 5. Official maximum system clock

RP2040:
* clk_sys and clk_peri max 133 MHz across all PVT at the default core voltage. RP2040 can run clk_sys at 200 MHz with an elevated core supply and VREG VSEL set to 1.15 V. [rp2040-datasheet.pdf section 2.15.3 note, page 179, txt lines 11528-11532, retrieved 2026-11-21]
* Front feature list: up to 133 MHz, or 200 MHz at 1.15 V. [rp2040-datasheet.pdf page 10, txt line 576, retrieved 2026-11-21]
* DVDD recommended 1.05-1.16 V, nominal 1.1 V. If using 200 MHz, set DVDD to 1.15 V. [rp2040-datasheet.pdf page 598, txt lines 39105-39118, retrieved 2026-11-21]
* The 200 MHz text was added in the 20 February 2025 datasheet revision. [rp2040-datasheet.pdf Appendix D.3, page 611, retrieved 2026-11-21]
* pico-sdk default SYS_CLK_HZ for RP2040 is 125 MHz. With PICO_USE_FASTEST_SUPPORTED_CLOCK=1 it is 200 MHz. [pico-sdk 2.3.1 src/rp2040/hardware_regs/include/hardware/platform_defs.h lines 74-92, retrieved 2026-11-21]
* Pico datasheet still lists the Pico as up to 133 MHz. [pico-datasheet.pdf page 3, txt line 158, retrieved 2026-11-21]

RP2350:
* clk_sys max 150 MHz. clk_peri 12-150 MHz. clk_hstx 150 MHz. [rp2350-datasheet.pdf section 8.1 clock table, page 472, txt lines 31515-31524, retrieved 2026-11-21]
* pico-sdk default SYS_CLK_HZ for RP2350 is 150 MHz. [pico-sdk 2.3.1 src/rp2350/hardware_regs/include/hardware/platform_defs.h line 116, retrieved 2026-11-21]
* Core regulator defaults to 1.1 V and is limited to 1.3 V unless VREG_CTRL.DISABLE_VOLTAGE_LIMIT is set. RP2350 might not operate reliably with DVDD other than 1.1 V. [rp2350-datasheet.pdf pages 406 and 419, txt lines 27497-27515 and 28278, retrieved 2026-11-21]
* The datasheet discusses overclocking from the ROSC but says there is not enough data to specify a safe setting. [rp2350-datasheet.pdf section 8.1.2.2.5, page 470, retrieved 2026-11-21]
* Pico 2: RP2350A at up to 150 MHz, 4 MB flash. [pico-2-datasheet.pdf pages 3-4, retrieved 2026-11-21]

Note: the TT Z80 test ran the RP2040 at 266 MHz and the TT firmware allows up to 250 MHz. Both are beyond the official ratings. [proven-projects.md 1.3; tt-micropython-firmware demoboard.py, retrieved 2026-11-21]

---------------------------------------------------------------------

## 6. RP2350 erratum E9

Where the exact text is: rp2350-datasheet.pdf Appendix D.5.1, pages 1358-1359, and rp/txt/rp2350-datasheet.txt lines 87712-87800. The datasheet is CC BY-ND 4.0. Fields below are copied where short and paraphrased where long.

* Reference: RP2350-E9. [rp2350-datasheet.pdf page 1358, retrieved 2026-11-21]
* Summary (verbatim): "Increased leakage current on Bank 0 GPIO when pad input is enabled". [same]
* Affects: RP2350 A2. [same]
* Fixed by: RP2350 A3, Documentation. [same]
* Description, paraphrased: on GPIO 0-47, when a pad is an input (IE set, output disabled, isolation clear) and its voltage sits between VIL and VIH, the pad sources leakage far above the IIN spec, typically about 120 uA at IOVDD 3.3 V. The leakage holds the pad near 2.2 V. The internal pull-down is too weak to overcome it. A driver or pull-down of 8.2 kOhm or less does overcome it, and once low the pad stays low. Disabling IE removes the leakage. Pull-up still works. At IOVDD 1.8 V it acts like an extra pull-up of about 30 uA between 0.6 V and 1.8 V. It does not affect the pull-down right after POR or RUN reset (IE starts clear). QSPI and USB pins are not affected. SWD pads are affected but pulled up by default. [same pages]
* Workaround, paraphrased: if you need pad pull-down, keep IE clear and enable it only around a read. Or fit an external pull-down of 8.2 kOhm or less. PIO cannot toggle pad controls, so PIO users may need external pulls. [same pages]
* Hardware fix: A3 changed the pad circuit to remove the leakage path. [rp2350-datasheet.pdf Appendix C.2.1, page 1349, retrieved 2026-11-21]

Steppings:
* Versions: A0 and A1 internal, A2 initial release, A3 internal development, samples and limited production, A4 production version. [rp2350-datasheet.pdf Table 7, page 21, retrieved 2026-11-21]
* Identify: A2 has CHIP_ID.REVISION 0x2. A3 has REVISION 0x3 and bootrom byte at 0x00000013 = 0x3. A4 has REVISION 0x3 and bootrom byte = 0x4. A4 has no hardware changes over A3, only bootrom changes. [rp2350-datasheet.pdf Appendix C.1-C.3, pages 1349-1350, retrieved 2026-11-21]
* PCN 28 (notification date 1 June 2025): mass production moves to A4 in July 2025 and all other variants are EOL. The top marking ends in "A4". Use pico-sdk 2.1 or newer. External circuits added for E9 may need review. [rp/RP235x-A4-stepping-PCN.pdf pages 2-4, retrieved 2026-11-21]
* Raspberry Pi news of 29 July 2025: A4 is the current production stepping, A2 stock was withdrawn, about 30,000 A3 units go into Pico 2 and Pico 2 W, A3 will not be sold to silicon customers. [https://www.raspberrypi.com/news/rp2350-a4-rp2354-and-a-new-hacking-challenge/, retrieved 2026-11-21]
* Current stepping today: A4. [datasheet Table 7 and PCN 28, retrieved 2026-11-21]
* Unknown: the stepping on any given TT ETR board. Check the top marking or read CHIP_ID and the bootrom byte. An early board with A2 would show E9. [inference]

Why E9 matters for asc (inference): during bus turnaround nobody may drive uio. On an A2 part with IE set, a floating uio line would sit near 2.2 V, in the undefined region, and the internal pull-down will not fix it. Use pull-ups, keepers, or always drive the bus. On A3 or A4 this does not apply.

---------------------------------------------------------------------

## 7. pico-sdk version current today

* Latest pico-sdk release: 2.3.1, published 2026-09-04. Earlier: 2.3.0 (2026-07-03), 2.2.0 (2025-07-29), 2.1.1 (2025-02-19). [https://api.github.com/repos/raspberrypi/pico-sdk/releases and https://github.com/raspberrypi/pico-sdk/releases/tag/2.3.1, retrieved 2026-11-21]
* 2.3.1 is a minor release: board additions, and a re-vetted pico_time and alarm pool with race fixes, plus RP2350 sync primitive fixes. [same release notes, retrieved 2026-11-21]
* The TT demo board does not use pico-sdk directly. It runs MicroPython with the TT SDK (tt-micropython-firmware, latest commit 15ba5ce on 2026-11-12). [https://tinytapeout.com/specs/pcb-etr/; repos/tt-micropython-firmware, retrieved 2026-11-21]

---------------------------------------------------------------------

## 8. Other RP2350 errata worth knowing (all steppings unless noted)

From rp2350-datasheet.pdf Appendix D, retrieved 2026-11-21:
* E2: SIO SPINLOCK writes mirrored at +0x80. Affects A2-A4. pico-sdk defaults PICO_USE_SW_SPIN_LOCKS to 1 on RP2350 unless PICO_XIP_RAM (pico-sdk 2.3.1 src/rp2_common/hardware_sync_spin_lock/include/hardware/sync/spin_lock.h lines 14-17).
* E5 and E8: DMA CHAIN_TO interactions with ABORT and with zero-length transfers. Affects A2-A4. Relevant if DMA feeds PIO for the bus.
* E27: bus priority controls apply to the wrong managers for APB and FASTPERI arbiters. Affects A2-A4.
* E4: RISC-V debug System Bus Access stalls while core 1 is in clock-gated sleep. Affects A2-A4.
* None of these is a GPIO or PIO timing erratum.

The Chip Against the Board Files (2026-11-25)
=============================================

This note checks the chip's side, line by line, against the files of the demo board of Tiny Tapeout: the top module and the pins in the chip's repository, [ttsky26e-almurut2](https://github.com/toksaitov/ttsky26e-almurut2), and the board files that Tiny Tapeout publishes. Two more independent checks, made the same day without seeing each other's answers, traced every one of the 26 pins and the 3 lines of the multiplexer through the pads of the frame, the package, the carrier's board file, the connector, and both demo boards, and found each number of [pins.h](../asc2-bridge/firmware/pins.h) right. Sources: [src/tt_um_toksaitov_almurut2.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/tt_um_toksaitov_almurut2.sv), [info.yaml](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/info.yaml), [tt-devboard-netlists.txt](tt-devboard-netlists.txt) (demo boards v3.2 and v3.3, and the SKY130 carrier), [bus-protocol-v1.md](bus-protocol-v1.md) (the rules for the devkit), [../asc2-bridge/firmware/pins.h](../asc2-bridge/firmware/pins.h) and [bridge.c](../asc2-bridge/firmware/bridge.c).

## The Pins

The names of the chip's [info.yaml](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/info.yaml) are the pins of the top module, and the carrier joins each pin of the chip to the contact of its name. Every pin goes to one GPIO of the RP2350B, the same on both boards. `make -C asc2-bridge/firmware/test` checks the numbers of [pins.h](../asc2-bridge/firmware/pins.h) against that [info.yaml](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/info.yaml) and both netlists, and fails on 16 wrong pins that were planted.

| Pin of the chip | Job | What else the boards join to it | Finding |
|---|---|---|---|
| `clk` | clock | v3.2: 47 Ohm in the line, and 10 kOhm to the network of the clock button. v3.3: nothing on the way. On both a solder bridge to pin 2 or 3 of the empty header J9, and a test point on the carrier | the RP2350 drives it; the network is too weak to disturb it. A clock source on that header would drive against it |
| `rst_n` | reset | v3.2: 100 nF to ground, 1 kOhm to a 10 kOhm pull-up and the reset button. v3.3: 1 kOhm to an inverter that holds it high unless the button is down | driven low by the RP2350 for a reset, let go afterwards; both boards hold it high by themselves |
| `ui_in[0]` ACK, `[1]` IRQ, `[2]` DBG_CK, `[4]` DBG_DI, `[5]` KBD_CLK, `[6]` KBD_DATA, `[7]` STOP | inputs | a DIP switch with 1 kOhm to 3.3 V, the input Pmod socket | driven by the RP2350. All switches off and the socket empty, as rule 7 says: a switch that is on takes 3.3 mA from the pin |
| `ui_in[3]` | free | the same | the chip reads it nowhere. The firmware drives it low: a pull-down alone can fail on the first version of the RP2350 (erratum E9) |
| `uio[0]` to `uio[7]` | bus | the bidirectional Pmod socket | no pull on the board; the RP2350 gives pull-ups, so a bus that nobody drives comes to 0xff, which is no status, within microseconds; just after a release it still holds the last byte. The handshake says when a byte counts, never the level |
| `uo_out[0]` REQ, `[1]` DBG_DO, `[2]` UIO_OE, `[3]` FRAME | outputs | a segment of the display behind 510 Ohm, the output Pmod socket | a load of 1.4 to 2.5 mA while high; the pads give 4 mA |
| `uo_out[4]` | free, high | the same | one segment is always lit. High as a silent serial line, since the boards put one there |
| `uo_out[5]` to `[7]` | free, low | the same | no current |

A Pmod board with outputs of its own in the input or the bidirectional socket would drive against the RP2350 or the chip (a QSPI memory board reads the bytes of the bus as its own commands), and a passive one adds load to the strobes. The VGA Pmod of Tiny Tapeout goes into the output socket, where the chip's outputs are REQ, DBG_DO, UIO_OE, and FRAME; the pictures of this computer come from the bridge's screen. The levels are the same on both sides: the pads of the chip and the RP2350 both work at 3.3 V.

## Tiny Tapeout's Own Firmware Agrees

The netlists come from the board files of [tt-demo-pcb](https://github.com/TinyTapeout/tt-demo-pcb), whose last change (2026-08-29, its Readme) is older than their retrieval, so they are the files of the boards that are sold. A second source, checked on 2026-11-26, gives the same numbers: Tiny Tapeout's MicroPython firmware for the demo board, [tt-micropython-firmware](https://github.com/TinyTapeout/tt-micropython-firmware) at commit 15ba5ced (2026-11-12), whose [gpio_map.py](https://github.com/TinyTapeout/tt-micropython-firmware/blob/15ba5ced/src/ttboard/pins/gpio_map.py) uses the map `GPIOMapTTDBv3` of [gpio_map_dbv3.py](https://github.com/TinyTapeout/tt-micropython-firmware/blob/15ba5ced/src/ttboard/pins/gpio_map_dbv3.py) for every board:

* the reset of the project on GPIO14 and its clock on GPIO16, the inputs `ui_in[0]` to `[7]` on GPIO17 to 24, the bidirectional pins on GPIO25 to 32, and the outputs `uo_out[0]` to `[7]` on GPIO33 to 40, as in [pins.h](../asc2-bridge/firmware/pins.h);
* the multiplexer on GPIO0 (enable), GPIO1 (reset of the counter), and GPIO2 (the pulses), as in [pins.h](../asc2-bridge/firmware/pins.h);
* the five pins of the analog header, ADC1 to ADC5, on GPIO41 to 45, which [pins.h](../asc2-bridge/firmware/pins.h) gives the button and the display;
* GPIO12 for the current source of the analog header and GPIO15 for the button of the manual clock, which [pins.h](../asc2-bridge/firmware/pins.h) leaves alone, and no use of GPIO46, where [pins.h](../asc2-bridge/firmware/pins.h) puts the side-set of the state machine.

## The Start

* A board comes with a firmware of its own (MicroPython). It selects the factory test and counts on the display, with the RP2350's pins of the bus as inputs. It cannot run this chip, and it harms nothing unless someone makes it drive the bus (`uio_oe_pico`). The UF2 of this project replaces it: BOOT held while the USB cable goes in. Tiny Tapeout's UF2 brings the old firmware back.
* Before any firmware runs the RP2350 drives nothing. The carrier pulls `ctrl_ena` and the reset of the multiplexer low with 1 kOhm, stronger than the 10 kOhm pull-ups of the board, so no design is selected.
* The firmware holds the reset low, resets the multiplexer, gives `TT_DESIGN` pulses on `ctrl_sel_inc`, and raises `ctrl_ena` (`design_select` in [bridge.c](../asc2-bridge/firmware/bridge.c)), in the order of rule 6. The line of the pulses has a pull-up of 5.1 kOhm and the firmware drives it low first.
* The chip then has its clock and the reset for at least 10 ms before the first release; it needs 20 clocks.
* A later reset counts its 20 clocks from 200 us after the RP2350 began to pull the line low, since v3.2 has 100 nF on it (found in the check of the release).
* The release is the one of rule 5: the clock stopped, the reset high, 200 us, the clock started. On v3.2 the edge takes about 17 us; the chip has no clock meanwhile. [tb/tt_um_powerup_tb.sv](../asc2-targets/tiny-tapeout/tb/tt_um_powerup_tb.sv) simulates this for both boards, with any value in the flops at power-up, and with the reset button pressed inside a request and inside an answer. It passes on the hardware description and on the gates of the layout.
* During the 200 us of the release the RP2350 drives the line high; with the button down at that moment 1 kOhm limits the current to 3.3 mA. After a release the pin of the reset is an input of the RP2350, so its output and the button never work against each other. On v3.2 a press and a release of the button are slow edges with the clock running; the filter of the chip takes them, and the bridge, which sees the line low, resets the chip itself when the button is up.

## Before a Board Is Switched On

* The eight switches off, the three Pmod sockets empty, no clock source on the header J9, no wire on the analog header (J16 on v3.2, J12 on v3.3). The firmware drives two of its pins from the start (GPIO44 and GPIO45, for a display), and two more (GPIO42 and GPIO43) when no display answers on I2C: a wire left there from an analog project would meet an output.
* The UF2 is the one built with the address of this chip on the shuttle. With a wrong address another design is selected, and the bus may meet its outputs; the first sign is that the first request is no fetch at 0x00800000.
* The other pins of the RP2350 stay alone: GPIO3 to 10 (management, the carrier's detection), GPIO11 (the board's LED), GPIO12 (on v3.3 the current source of the header J11, whose resting level switches it fully on, harmless while nothing is on that header), GPIO15 (on v3.3 the clock button, an output of an inverter, so never drive it).
* The buyer does not choose between v3.2 and v3.3, so both must work. The older RP2040 board and the FPGA board have other pins.

## What No Check Here Can Show

* The firmware has not run on an RP2350. The window of 32 pins from GPIO16 for the state machine comes from the datasheet and from the code of the SDK.
* The delays of the multiplexer and the pads are put into the FPGA by a switch ([asc2-targets/de10-lite/pin_delay.sv](../asc2-targets/de10-lite/pin_delay.sv), SW7), and that FPGA build has not run on the board yet.
* The address of the chip on the shuttle (`TT_DESIGN`) is known only after the closing.

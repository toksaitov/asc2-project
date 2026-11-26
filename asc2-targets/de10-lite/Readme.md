Almurut II on the DE10-Lite
===========================

This directory contains the Quartus project that puts the chip of Almurut II into the MAX 10 FPGA of a Terasic DE10-Lite board. The FPGA holds the Tiny Tapeout top module unchanged, and a Raspberry Pi Pico on jumper wires does the work of the RP2040 of a Tiny Tapeout board: it gives the clock and the reset and serves memory and devices over the bus. The board runs the whole computer on the bench, with the same top module as the chip on the shuttle.

## Layout

* [asc2.sv](asc2.sv): the top module of the board. It gives the pins of the chip the pins of the board. Its header comment lists every wire to the Pico.
* [pin_delay.sv](pin_delay.sv): the delay of a Tiny Tapeout board for the pins of the chip.
* [files.f](files.f): the two modules of the board. The Makefile adds the files of the chip after them, in the order of the chip's [info.yaml](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/info.yaml).
* [asc2.qsf](asc2.qsf), [asc2.qpf](asc2.qpf), [asc2.sdc](asc2.sdc): the Quartus project, with the places of the pins and the clock. `files.qsf` is made from [files.f](files.f).
* [tb/asc2_tb.sv](tb/asc2_tb.sv): a test of the top module against a small model of the Pico, with a model of the PLL ([tb/altpll.sv](tb/altpll.sv)) and a watcher of the delays ([tb/delay_watch.sv](tb/delay_watch.sv)).
* [Makefile](Makefile): the build, the upload, lint, and the test.

The chip itself, with its top module, is in its own repository, [ttsky26e-almurut2](https://github.com/toksaitov/ttsky26e-almurut2), and the Makefile reads it from a clone beside this repository ([../../chip.mk](../../chip.mk)). The firmware of the Pico is in [../../asc2-bridge/](../../asc2-bridge/).

## What the Board Adds to the Chip

* The clock of the chip is chosen by the slide switch `SW8`. Down: the 50 MHz clock of the board divided by 4, 12.5 MHz. Up: the clock wire from the Pico, as on a Tiny Tapeout board, where the RP2040 gives the clock. Move the switch only while the Pico holds the chip in reset.
* The slide switch `SW7` puts the delays of a Tiny Tapeout board between the pins and the chip. Up: every input of the chip but its clock and every output comes 10 to 15 ns later, as through the pads and the multiplexer of a Tiny Tapeout chip, for which Tiny Tapeout names up to about 10 ns each way ([../../asc2-kb/tt-io.md](../../asc2-kb/tt-io.md)). Down: no delay. Move this switch too only while the Pico holds the chip in reset. The clock has no delay: the handshake of the bus does not use its phase.
* STOP has two sources: the slide switch `SW9` and a wire from the Pico. While the switch is up or the wire is high, the CPU waits between two instructions. The monitor of the bridge uses the wire for its commands `stop` and `step`.
* The two lines of the keyboard are on pins 6 and 26 of the header, for two wires from the Pico, which plays the keyboard. Both pins have pull-ups in the FPGA: without the wires the chip sees a keyboard that sends nothing.
* The debug port of the chip is on pins 1, 3, and 5 of the header, for three wires from the Pico, whose command `regs` reads it. Its two inputs have pull-ups in the FPGA: without the wires the clock of the port never rises.
* A guard on the bus. The FPGA and the Pico both drive the eight wires of the bus, each in its turn. The Pico raises one more wire while it drives, and the FPGA keeps its drivers off during that time. The chip has no such input; the guard keeps a mistake on the bench from setting two outputs against each other.
* The red LEDs show the wires: `LEDR0` REQ, `LEDR1` ACK, `LEDR2` the chip drives, `LEDR3` the Pico drives, `LEDR4` FRAME, `LEDR5` IRQ, `LEDR6` STOP, `LEDR7` the clock is the Pico's, `LEDR8` the delays are on, `LEDR9` the reset.

## The Wires

All wires to the Pico are on one row of the 40-pin header `JP1` of the board: the row of the even pins, which is the one at the edge of the board. Every pin of that row has its wire:

```
pin    2    4    6     8    10   12   14   16   18   20   22   24   26     28   30   32   34   36    38      40
wire   RST  CLK  KBD   D0   D1   GND  D2   D3   D4   D5   D6   D7   KBD    REQ  GND  ACK  IRQ  STOP  PICO    CHIP
                 CLOCK                                              DATA                              DRIVES  DRIVES
Pico   GP0  GP1  GP16  GP2  GP3  GND  GP4  GP5  GP6  GP7  GP8  GP9  GP17   GP10 GND  GP11 GP12 GP13  GP14    GP15
```

The row of the odd pins has the three wires of the debug port, on its first three pins, next to RST, CLK, and KBD CLOCK of the other row:

```
pin    1         3         5         7  9  11   ...  29
wire   DEBUG     DEBUG     DEBUG     -  -  5 V       3.3 V
       CLOCK     DATA IN   DATA OUT
Pico   GP18      GP19      GP20
```

The rest of that row stays empty. Its pins 11 and 29 are the 5 V and the 3.3 V of the board (the manual of the DE10-Lite, figure 3-18), and a wire of the Pico on either of them can destroy a pin of the Pico; pins 7 and 9 lie between the debug port and the 5 V pin. Every wire is a plain jumper wire without a resistor.

The two wires of the keyboard, on pins 6 and 26, are the only ones that the bus does not need. Without them the chip sees a keyboard that sends nothing, and everything works but the `key` command of the bridge.

`SW8` chooses the clock: down, the clock of the board; up, the clock of the Pico. Both work since another jumper wire went into the place of the clock wire on 2026-11-23: two runs of `make board-suite` with the Pico's clock, 123 million bus transactions, and none broke off. The first clock wire lay next to the wires of the bus, and when the FPGA changed all eight of them at once the wire showed the FPGA an edge too many. On 2026-11-23 the kernel of [../../asc2-rach2/](../../asc2-rach2/) booted seven times with the Pico's clock and four times with the clock of the board, about 7 million bus transactions per boot. With the Pico's clock over the first wire 3 to 7 transactions per boot broke off; the chip repeated each one, and every boot printed what it must. With the clock of the board none broke off. [../../asc2-kb/bridge-design.md](../../asc2-kb/bridge-design.md) has the details. The `clock` command of the bridge changes the speed of the Pico's clock, and the programs ran with it from 6.25 MHz to 37.5 MHz.

## Checking

```bash
make lint-local
make test
```

`make lint-local` runs Verilator's lint with `-Wall` on the board's modules and the chip. `make test` serves a program of three instructions over the shared wires and checks the reset, the two sources of STOP, a step of one instruction as the Pico makes it, the guard, the wires of the keyboard, the wires of the debug port, and the LEDs, with each of the two clocks, and then the same again with the delays. In every run it measures the time that the watched pins take. Both need Verilator 5 and no Quartus.

## Building and Loading

The build needs Quartus Prime Lite (24.1std or newer) with `quartus_map`, `quartus_fit`, `quartus_asm`, `quartus_sta`, and `quartus_pgm` in the `PATH`, GNU Make, and a shell. With them:

```bash
make build
make upload
```

`make build` makes `build/asc2.sof`, and `make upload` loads it into the FPGA over JTAG. The design is gone when the board loses power. Nothing here writes the flash of the board, and the build deletes the file that Quartus makes for it.

`make upload` names the cable `USB-Blaster [USB-0]`. `jtagconfig` lists the cables; a DE10-Lite shows that name and a device with the JTAG ID `0x031050DD`.

Quartus does not run on macOS. In this project it runs on a Windows computer that is reached over SSH under the name `quartus`. The project names the files of the chip by relative paths, so the build there needs copies of both repositories, side by side, as they are committed:

```bash
cd ../..
git archive --prefix=asc2-project/ -o /tmp/asc2-project.tgz HEAD
git -C ../ttsky26e-almurut2 archive --prefix=ttsky26e-almurut2/ -o /tmp/ttsky26e-almurut2.tgz HEAD
scp /tmp/asc2-project.tgz /tmp/ttsky26e-almurut2.tgz quartus:
ssh quartus 'rm -rf asc2-project ttsky26e-almurut2 && tar xzf asc2-project.tgz && tar xzf ttsky26e-almurut2.tgz && rm asc2-project.tgz ttsky26e-almurut2.tgz'
ssh quartus 'cd asc2-project/asc2-targets/de10-lite && make build && make upload'
```

The address of that computer and its key are in `~/.ssh/config` and not in the repository. Any computer with Quartus serves in the same way.

Almurut II on Tiny Tapeout
==========================

This directory holds the checks of the chip for a Tiny Tapeout shuttle on the SKY130 process: lint, the Verilator tests of the pins, the start on the demo board, the layout run, and the tests on the results of other tools. The overview of the whole platform is in [../../Readme.md](../../Readme.md).

The chip is the Tiny Tapeout project in its own repository, [ttsky26e-almurut2](https://github.com/toksaitov/ttsky26e-almurut2), in the shape of the official template, and the Makefile here reads it from a clone beside this repository ([../../chip.mk](../../chip.mk)). That repository holds:

* [info.yaml](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/info.yaml): the description of the project for Tiny Tapeout: the size in tiles, the clock, the top module, the source files in their order, and the names of the pins.
* [src/tt_um_toksaitov_almurut2.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/tt_um_toksaitov_almurut2.sv): the top module. Its header comment lists the job of every pin. It joins three parts: the filters of the reset and of the interrupt line, the bus unit, and the CPU with its devices.
* `src/asc_*.sv`: the chip itself (see The Files of the Chip).
* [src/config.json](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/config.json): the settings of the layout tools, those of the Tiny Tapeout template with the changes that a comment in the file marks, and [src/signoff.sdc](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/signoff.sdc), the clock of the last timing report, 80 ns.
* [docs/info.md](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/docs/info.md): the page of the chip in the datasheet.
* [test/](https://github.com/toksaitov/ttsky26e-almurut2/tree/main/test): the cocotb test of the template, which GitHub runs on the hardware description and on the gates, and the workflows of the template in [.github/workflows/](https://github.com/toksaitov/ttsky26e-almurut2/tree/main/.github/workflows).

## Layout

* [tb/tt_um_tb.sv](tb/tt_um_tb.sv): a test of the pins. A small model of the bridge serves a program of six instructions, and the test checks the reset, the pin that holds the CPU, an interrupt, and the debug port.
* [tb/tt_um_reset_tb.sv](tb/tt_um_reset_tb.sv): a test of the reset for a simulator that starts every flop as unknown.
* [tb/tt_um_powerup_tb.sv](tb/tt_um_powerup_tb.sv): a test of the start on the demo board of Tiny Tapeout, for both revisions of the board.
* [tb/asc_trace_gates_top.sv](tb/asc_trace_gates_top.sv): the place of the gates in the test program of [../../asc2-tests/chip/](../../asc2-tests/chip/), for whole programs on the gates.
* [precheck/Dockerfile](precheck/Dockerfile): the container of the official precheck.
* [Makefile](Makefile): lint, the tests, the layout run, the precheck, and the tests on the results of other tools.

[../../asc2-tests/chip/](../../asc2-tests/chip/) runs whole programs on the top module, and [../../asc2-tests/bus/](../../asc2-tests/bus/) tests the bus unit alone. Both read the chip from the same clone.

## The Files of the Chip

The rules of the machine are in the comments of [../../asc2-emulated/asc.h](../../asc2-emulated/asc.h). The files of the chip follow them and do not repeat them, and [../../asc2-tests/chip/](../../asc2-tests/chip/) checks that the chip does what the emulator does. The files in its [src/](https://github.com/toksaitov/ttsky26e-almurut2/tree/main/src), in an order to read them in:

* [asc_soc.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_soc.sv): the CPU with its devices, the divider of the timer's clock, and the debug port. Its header comment has a picture of the parts and the list of the debug port's bits.
* [asc_cpu.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_cpu.sv): the CPU, a state machine that does one instruction in several clocks. Its header comment names the states and the registers.
* [asc_decoder.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_decoder.sv): tells whether a word is an instruction and whether the instruction is privileged.
* [asc_regs.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_regs.sv): the register file.
* [asc_alu.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_alu.sv): the arithmetic and logic unit, with the one adder of the CPU.
* [asc_tlb.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_tlb.sv): the eight translations that the CPU remembers.
* [asc_icache.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_icache.sv), [asc_word.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_word.sv): the instruction cache of 32 words.
* [asc_dot.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_dot.sv), [asc_dot_cell.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_dot_cell.sv): the AI accelerator, a row of four cells that three rows of weights and inputs go through one after another, for `dot4`, `dotv`, `dotm`, `learn`, and `xsh`.
* [asc_pic.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_pic.sv), [asc_pit.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_pit.sv), [asc_kbdc.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_kbdc.sv): the interrupt controller, the timer, and the keyboard controller.
* [asc_ps2.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_ps2.sv): the receiver of the keyboard's two wires.
* [asc_bus.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_bus.sv), [asc_reset_filter.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_reset_filter.sv), [asc_irq_filter.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_irq_filter.sv): the bus unit, which sends every memory access to the bridge one byte at a time, and the filters of the reset and interrupt inputs. [../../asc2-kb/bus-protocol-v1.md](../../asc2-kb/bus-protocol-v1.md) describes the bus, and the header comment of [asc_bus.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_bus.sv) the unit.

[../../asc2-kb/cpu-design.md](../../asc2-kb/cpu-design.md) tells why the CPU is built this way. A few conventions hold in every file:

* There is no package of shared constants. Each file holds the constants that it needs as `localparam`, because the Tiny Tapeout tools read the sources with a Yosys that knows no package import.
* The files have no `timescale` line: the Tiny Tapeout tools warn about every cell of the chip otherwise, and a simulation sets the time unit on its command line.
* `ASC_TEST_TICK` and `ASC_BUS_TEST_TEAR` are defined by tests only. Each adds inputs that a test needs; [asc_soc.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_soc.sv) and [asc_bus.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_bus.sv) describe them. The chip is built without them.

## Checking

```bash
make
```

This runs Verilator's lint on the chip, the test of the pins, which prints `passed: tt_um_tb`, and the test of the start on the demo board. It needs Verilator 5.

## The Start on the Demo Board

```bash
make powerup
```

The chip sits on the demo board of Tiny Tapeout, behind the multiplexer that selects one design of the shuttle, next to an RP2350 that gives it the clock and the reset. [tb/tt_um_powerup_tb.sv](tb/tt_um_powerup_tb.sv) copies what the board does to the pins of the chip ([../../asc2-kb/tt-devboard.md](../../asc2-kb/tt-devboard.md) has the sources):

* A design that is not selected gets no clock, and its reset and its inputs are low. Its flops hold what the power has left in them: every run gives each flop a value by chance.
* The stock firmware of the board selects a design with the reset high and the clock running. The chip then runs without ever having had a reset, and nothing can be expected of it. The reset that follows must bring every pin to its level within 11 clocks.
* The firmware of this project follows the rules of [../../asc2-kb/bus-protocol-v1.md](../../asc2-kb/bus-protocol-v1.md): the reset low for 20 clocks, the clock stopped, the reset released, 200 us, the clock started.
* The reset pin differs between the two revisions of the board. On v3.2 a capacitor makes its level rise and fall slowly, so the chip may see many edges where the RP2350 or the reset button has made one: the test gives the pin levels by chance for as long as the level is on its way. On v3.3 an inverter gives one clean edge, at any moment of the clock.
* The board has a reset button. The test presses it inside a request, with the acknowledge line high, and again while the RP2350 gives the bytes of an answer. The chip must begin anew with its first fetch each time.
* While nobody drives the bus its pins read low, high, or a level that changes by chance: the test runs once for each of the three.

Throughout the run the chip and the RP2350 must never drive the bus together. The test also prints how many clocks after the release of the reset the chip first drives the bus, which is 15; the firmware has that long to let go of the bus when it sees the reset.

The target runs the test 24 times: two revisions, three levels of the free pins, four seeds. `make gates` runs it on the gates of a layout run as well.

15 mistakes of the reset were planted in the hardware description, one at a time, and the test failed on each: a reset that leaves the drivers of the bus on, that leaves REQ, FRAME, the sequence bit, the state of the bus unit, `ip`, the state of the CPU, the halted flop, the double-fault flop, the lock of the debug port, the flags, or `cr0` as it was, a reset that takes the acknowledge line as low whatever its level, a STOP pin that does not hold the CPU, a reset without its filter, and a reset that needs twice as many clocks. One more changes nothing: the mark of a sequential fetch needs no reset, because every fetch carries its address after a reset, and the write of `cr0` that changes that clears the mark.

## The Layout Run

```bash
make harden
```

This copies the project from the clone of the chip to `build/project/`, as GitHub checks it out (the files that git keeps), and runs the official tools on it: lint, synthesis, placement, routing, and the checks of the result. It needs Docker and the Tiny Tapeout tools in `~/ttsetup/`; [../../asc2-kb/tt-flow.md](../../asc2-kb/tt-flow.md) tells how they were set up. The run takes well over an hour, nearly all of it for the routing.

It ends with the use of the area, the number of cells, and the warnings of the tools. The log is `build/project/harden.log`, and the reports are in `build/project/runs/wokwi/`.

The chip's [info.yaml](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/info.yaml) holds its size in tiles. Another size is tried without a change of the file:

```bash
make harden TILES=6x2 PROJECT_DIR=build/project-6x2
```

[../../asc2-kb/tt-flow.md](../../asc2-kb/tt-flow.md) has the results for the sizes that were tried, in its section 14. A run in another directory leaves the layout in `build/project/` as it is.

## The Test on the Gates

```bash
make gates
```

This runs the test of the pins on the gates that the last layout run made of the chip, with the Verilog models of the cells of the library and without delays. The test runs eight times; each time the flip-flops start with other values, as they do when the real chip gets power. The test of the start on the demo board follows, 24 runs on the same gates. A layout run in another directory is named as for `make harden`, with `PROJECT_DIR`.

Then Icarus Verilog runs the test of the reset on the same gates. Icarus Verilog starts every flip-flop as unknown, and the chip has to reach known values by its reset alone and ask for its first instruction. It needs Icarus Verilog 13.

```bash
make gates-programs
```

This runs whole programs on the same gates. The test program of [../../asc2-tests/chip/](../../asc2-tests/chip/) plays the RP2040 for the gates as it does for the hardware description; [tb/asc_trace_gates_top.sv](tb/asc_trace_gates_top.sv) stands between them. The gates do not show the registers of the CPU, so the chip runs on its own time, and what leaves its pins is compared with the emulator.

* The programs of [../../asc2-tests/asm/](../../asc2-tests/asm/) that end by themselves and need no input and no timer, 27 of the 36: every request that the gates send on the bus must be the request of the emulator, in the same order. A program that takes another way, or that reads or writes another address or value, sends other requests. Each program runs from two sets of values in the flip-flops.
* The kernel: it boots from its disk, the program `array` of its tests runs from the shell (two processes use the array of the dot products while the timer switches between them), and the console must show what it shows on the emulator.

A clock of the gates takes about 50 times as long as a clock of the hardware description: the programs take about five minutes and the kernel about twenty. Neither needs Docker: the netlist of the last layout run is enough.

```bash
make sv2v
```

This runs both tests on the plain Verilog that `sv2v` makes of the sources. It needs `sv2v` and Icarus Verilog.

`make project` only copies the project. The tools refuse a project that is not a git repository with a remote, so `build/project/` gets a repository of its own with the address in `PROJECT_REMOTE`; nothing contacts that address.

## The Submitted Layout

* `tiles` in the chip's [info.yaml](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/info.yaml) says 8x2, 16 tiles. The layout fills 63.2 % of them ([../../asc2-kb/array-design.md](../../asc2-kb/array-design.md), "The Layout").
* `clock_hz` says 12.5 MHz, the fastest clock at which the bus was tried on the bench. `CLOCK_PERIOD` in [config.json](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/config.json) asks the tools for 20 ns, four times as fast: the typical corner meets it, and the slowest corner misses it by 6.1 ns, which does not matter at 12.5 MHz. The clock comes from the microcontroller of the board.
* GitHub runs the same layout tools on every push to the chip's repository, and the address of that repository goes to app.tinytapeout.com.

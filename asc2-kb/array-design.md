The Array of the Dot Products: Design and Measured Gain (2026-11-25)
====================================================================

The array speeds up neural networks of various kinds, in the kernel and in programs, against the plain CPU, within 16 tiles (2026-11-25). This note describes what was built for it in the emulator, the assembler, and the kernel (step 1), and what the measurement says. The reference of every rule is [../asc2-emulated/asc.h](../asc2-emulated/asc.h) ("Array of the Dot Products") and [../asc2-emulated/Readme.md](../asc2-emulated/Readme.md). The same in the hardware description, on the simulated chip, and on the FPGA board is step 2: "The Hardware" and "The Time in Clocks and on the Board" below.

## What the Chip Had Before

One instruction, `dot4 ra rb rd`: it takes two registers as four signed bytes each, multiplies the bytes in the same places, and adds the four products to a third register. The unit behind it is a row of four cells; a cell multiplies two signed bytes and adds the product to the sum that passes through the row (a systolic array: equal cells in a line, each one handing its result to the next). The weights and the inputs live in the ordinary registers, and the unit keeps nothing between two instructions.

## What Is New

The array gets three rows of four weights and four inputs, registers of its own, and the cells of one row work through the rows one after another (cells for every row did not fit; "The Layout"):

* `w0`, `w1`, `w2`: the weights of the three rows, four signed bytes each;
* `x0`, `x1`, `x2`: the inputs of the three rows, four signed bytes each.

The first cell of a row is the low byte of its register. The values stay in the array from one instruction to the next. That is the point of the change: a network or a filter of up to twelve weights lives in the chip, and its weights do not cross the bus for every use.

| Instruction | What it does | What it is for |
| ----------- | ------------ | -------------- |
| `mov rN wK`, `mov rN xK`, and back | moves a word between a register and a register of the array | loading weights and inputs, reading weights that have been learned |
| `dotv rd` | adds all twelve products to `rd`; the flags tell if the new `rd` is zero or negative | a neuron of twelve inputs; a jump on its sign is a prediction |
| `dotm rd` | adds the four products of each row to a register of its own: `rd`, `rd+1`, `rd+2` | three neurons of four inputs at once (a layer); a matrix of three rows times a vector |
| `learn ra` | byte k of `ra` is the error of row k in 128ths; every weight gets its input times that error, rounded, and stays within a signed byte | learning in place (the delta rule of a perceptron) |
| `xsh ra` | the twelve inputs move on by one cell, the last one is dropped, the first cell takes the low byte of `ra` | a filter or a convolution over a stream; a history of events |
| `ssat rd`, `usat rd` | holds `rd` within -128 to 127, or within 0 to 127 | the output of a neuron as the input of the next layer; `usat` is the rectified linear unit (ReLU) |
| `popcnt ra rd` | counts the bits of `ra` that are set | networks with weights and inputs of one bit: 32 inputs with one instruction |

`dot4` stays as it was. It uses no register of the array, so the kernel's scheduler network does not disturb a program. The number of rows is one constant (`CPU_DOT_ROWS`, 1 to 4), three here.

## How the Kernel Shares the Array

The chip has one set of the six registers, and every process needs its own values. The kernel does what the kernels of the PC do with the registers of the floating-point unit ([../asc2-rach2/sys/proc/array.c](../asc2-rach2/sys/proc/array.c)). The bit A of `cr0` is the switch: while it is clear, an instruction of a user program that names a register of the array raises the privileged instruction exception and changes nothing. The kernel remembers the owner, the process whose values are in the chip. The owner runs with the bit set. The first instruction for the array of any other process enters the kernel, which stores the six registers for the old owner, loads those of the process, and lets the instruction run again. A process that never uses the array costs nothing at a switch. A program starts with zeros, and a child of `fork` has the values of its parent.

## The Lock of the Debug Port

The lock must not lock the whole machine out. A write of the value 0x4c4f434b (the letters "LOCK") to `cr8` in kernel mode locks the debug port; any other value does nothing; `cr8` reads 1 while the port is locked; only a reset opens it. The lock hides the read-out of the registers only. The bus, the clock, the reset, and the STOP pin work as before, so a locked chip is never a dead chip: a reset gives the port back. The emulator has no debug port, so it checks the rules of `cr8`. The effect on the port is checked on the simulated chip and on the board ("The Hardware" below).

## What Was Measured

Every instruction and every word of data that the chip does not hold is a request on the bus to the RP2040, and a request takes far longer than the work of the chip between two requests. On the bench the chip runs about 29,000 instructions in a second while every one of them is fetched over the bus. So the count of the requests is the measure of the time.

`make gain` in [../asc2-tests](../asc2-tests) writes six small jobs in assembly, each with the old instructions (as well as they can be used) and with the new ones, and counts the requests for one unit of work. Every value that a job stores is compared with a model in Python. The programs in the old style were also assembled and run with the tools of `master`: the requests are the same, byte for byte.

| One unit of work | Chip of `master` | Old code, with the cache | New code, with the cache | Gain |
| ---------------- | ---------------- | ------------------------ | ------------------------ | ---- |
| a neuron of a layer, 12 inputs, weights in memory | 21.5 requests (17 instructions) | 4.0 | 4.0 (15 instructions) | 5.4 times |
| a guess of a network of 4 inputs, 3 neurons, 1 output, held in the chip | 38.6 (37) | 17.5 | 2.0 (25) | 19.3 times |
| an output of a filter of 12 taps | 22.0 (20) | 2.0 | 2.0 (9) | 11.0 times |
| a vertex times a matrix of 3 rows | 18.0 (14) | 4.0 | 4.0 (15) | 4.5 times |
| a neuron of 256 inputs of one bit | 223.0 (206) | 17.0 | 17.0 (94) | 13.1 times |
| a step of learning of a neuron of 12 inputs | 261.0 (217) | 72.0 | 5.0 (21) | 52.2 times |

## What the Numbers Say

* The instruction cache of 32 words brings most of the gain. A loop that fits it fetches nothing, and only the data crosses the bus.
* The array adds to the cache where the work stays in the chip: learning (72 requests become 5) and a small network held whole (17.5 become 2). The old code of both is too long for the cache; the new code fits.
* For a layer whose weights come from memory, and for a vertex times a matrix, the array adds nothing to the cache: 4 requests are the floor, because every word of weights or of results must cross the bus once. No instruction can change that; only a wider bus could.
* For the filter and the one-bit neuron the requests are equal with and without the new instructions, and the instructions are half as many. That shows in time only on a faster bus than that of the bench.
* These are requests, not seconds. The next section has the time.

## The Time in Clocks and on the Board

Measured on 2026-11-25 with the same six jobs ([../asc2-tests/gain/gain_time.py](../asc2-tests/gain/gain_time.py)), on the array that is built: one row of cells that the three rows of weights and inputs go through one after another ("The Hardware"). Each program runs with two amounts of work, and the difference is the time of one unit of work.

On the board (`make board-gain` in [../asc2-tests](../asc2-tests)) the chip in the FPGA runs at 12.5 MHz, and a program reads the clock of the bridge, in microseconds, at its start and at its end:

| One unit of work | Old code, no cache | Old code, with the cache | New code, with the cache | Gain | Gain of cells for every row |
| ---------------- | ------------------ | ------------------------ | ------------------------ | ---- | --------------------------- |
| a neuron of a layer, 12 inputs, weights in memory | 212.9 us | 47.3 | 46.2 | 4.6 times | 4.8 |
| a guess of a network of 4 inputs, 3 neurons, 1 output, held in the chip | 391.5 | 191.6 | 39.4 | 9.9 times | 10.4 |
| an output of a filter of 12 taps | 225.0 | 36.0 | 24.1 | 9.3 times | 10.1 |
| a vertex times a matrix of 3 rows | 177.9 | 45.9 | 46.5 | 3.8 times | 4.0 |
| a neuron of 256 inputs of one bit | 2,255.4 | 311.5 | 226.7 | 10.0 times | 9.9 |
| a step of learning of a neuron of 12 inputs | 2,535.2 | 749.7 | 65.4 | 38.8 times | 40.8 |

On the simulated chip (`make gain` in [../asc2-tests/chip](../asc2-tests/chip)) the test counts the clocks while the chip runs on its own time. With an RP2040 that answers at once, the count is the work of the chip alone:

| One unit of work | Old code, no cache | Old code, with the cache | New code, with the cache | Gain | Gain of cells for every row |
| ---------------- | ------------------ | ------------------------ | ------------------------ | ---- | --------------------------- |
| a neuron of a layer, 12 inputs, weights in memory | 1,634 clocks | 423 | 408 | 4.0 times | 4.2 |
| a guess of a network of 4 inputs, 3 neurons, 1 output, held in the chip | 3,014 | 1,552 | 406 | 7.4 times | 7.8 |
| an output of a filter of 12 taps | 1,752 | 372 | 222 | 7.9 times | 8.8 |
| a vertex times a matrix of 3 rows | 1,373 | 407 | 416 | 3.3 times | 3.5 |
| a neuron of 256 inputs of one bit | 17,355 | 3,141 | 2,077 | 8.4 times | 8.4 |
| a step of learning of a neuron of 12 inputs | 19,381 | 6,340 | 597 | 32.5 times | 34.8 |

What the two tables say:

* The simulated chip and the board agree. With an RP2040 that answers a change of REQ after 4 clocks, the simulated chip takes 572 clocks for the neuron of the layer with the new code, which is 45.8 us at 12.5 MHz; the board measures 46.2 us. The old code without the cache takes 2,570 clocks, 205.6 us, against 212.9 us on the board.
* The gain in time is smaller than the gain in requests (3.8 to 39 times against 4.5 to 52 times), because the chip works on one bit in a clock, and its own work is no longer small beside the few requests that are left. At 12.5 MHz about three quarters of the time of the new code pass in the chip, and the rest in waiting for the RP2040.
* Against cells for every row, the shared cells cost 22 clocks for `dotv` and `dotm` and 18 for `learn`: 4 to 9 % of the time of five jobs on the board, and nothing for the job of one-bit weights, which does not use them. The last column of the tables holds the gain with cells for every row, measured before. The cache alone gives the two jobs that load weights from memory most of their gain: the vertex with the array (416 clocks, 46.5 us) is as slow as the old code with the cache (407 clocks, 45.9 us), and the neuron of a layer is 2 to 4 % faster. The array pays where the weights stay in the chip: the small network, the filter, the bits, and the learning, where it is 1.4 to 11.5 times faster than the cache alone.
* So a faster clock of the chip now pays, which it did not while every instruction was fetched over the bus. The layout of step 3 says how fast the chip can run.
* "Old code, no cache" is the old code on the new chip with the cache off. Its requests are those of the chip of `master`, byte for byte; its clocks were not compared with `master`.

## The Hardware

Step 2 of the listed steps, built on 2026-11-25. The array is [asc_dot.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_dot.sv) in the chip's repository (the one row of cells, the rows of the registers of the weights and the inputs, and the selectors between them; the parameter `ROWS` is the number of rows) and [asc_dot_cell.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_dot_cell.sv) (one cell). A cell multiplies two signed bytes in eight clocks with one adder, one bit of a factor in a clock, as the cell of `dot4` did. The four cells multiply the bytes of one row at the same time; then the sums pass down the row, one cell in a clock, the CPU adds the sum to a register, and the cells go on with the next row. A row takes 14 clocks of `dotv` or `dotm`, and 9 of `learn`. `learn` uses the same cells and adders: a cell starts from its weight and multiplies its input by the error of its row, so learning adds no multiplier. `popcnt` uses the adder of the CPU and counts one bit in a clock.

The registers of the array have no reset, to save area. A program must write them before it reads them; the kernel gives every process zeros.

The debug port grew from 640 to 643 bits. Bit 640 tells that the port is locked, and bits 641 and 642 are the bits C and A of `cr0`, so the port shows all of `cr0`. A locked port shows a 1 in bit 640 and zeros in all other bits. The port does not show the six registers of the array; a program can move them to ordinary registers, which the port shows. The command `regs` of the bridge prints `locked=1` and no register for a locked port.

Sizes, before the layout (synthesis for sky130, cells only):

| Design | Area |
| ------ | ---- |
| before the array (cache of 32 words, one row for `dot4`) | 117,089 um2 |
| the array as built: four cells, three rows of registers | 129,163 um2 |
| the same with four rows of registers (estimated) | 131,815 um2 |
| two rows of cells and registers | 132,044 um2 |
| three rows of cells and registers | 141,997 um2 |
| four rows of cells and registers | 151,624 um2 |

One cell takes 1,594 um2. The layout of the two larger variants of the array is in "The Layout". On the FPGA (DE10-Lite) the design takes 5,821 logic elements and 2,784 registers (6,387 and 2,928 with cells for every row) and meets its timing with the clocks of the bench, 12.5 MHz from the board and up to 25 MHz from the Pico (the tool gives 36.7 MHz as the limit of the design).

The start of the chip on the demo board is simulated for both versions of the board ([../asc2-targets/tiny-tapeout/tb/tt_um_powerup_tb.sv](../asc2-targets/tiny-tapeout/tb/tt_um_powerup_tb.sv), `make powerup`): the board selects the design while its clock runs, the firmware starts it by the rules of [bus-protocol-v1.md](bus-protocol-v1.md), and the reset button is pressed in the middle of a request and in the middle of an answer. Version 3.2 of the board has a slow reset line that chatters, version 3.3 a clean one. Two facts for the firmware of step 4 came out of it: the chip first drives the bus 15 clocks after the reset has ended, and the firmware must let go of the bus as soon as it sees the reset.

## The Layout

Step 3 of the listed steps, on 2026-11-25, on 16 tiles (8x2, 302,420 um2) with the 20 ns clock target of the template. Four variants were laid out with `make harden` of [../asc2-targets/tiny-tapeout](../asc2-targets/tiny-tapeout) on the Tiny Tapeout toolchain (LibreLane 3.0.14):

| Design | Density | Filled | Routing | Hold (worst corner) | Antenna | Result |
| ------ | ------- | ------ | ------- | ------------------- | ------- | ------ |
| three rows of cells | 72 | 67.8 % after the timing repair | not reached | | | the placement of the diodes fails, with the clock target of 20 ns and with 40 ns |
| two rows of cells | 70 | 65.6 % | 0 errors | +0.047 ns | 1 wire | passes the flow; with a hold margin of 0.1 one path missed the hold time of the fastest corner by 0.0098 ns, with 0.15 it does not |
| shared cells, 3 rows of registers | 70 | 61.8 % before routing | 39,463 errors after four passes | | | stopped: the placer spreads the cells apart (target fill 97 %), and the wires are 28 % longer than those of two rows |
| shared cells, 3 rows of registers, final | 73 | 63.2 % | 0 errors | +0.079 ns | 0 | every check passes |

For the shared cells two more settings were tried and stopped before they ended: density 66 (during the timing repair, its wires already longer), and density 75 without the placer's spreading for routability (in the first pass of the routing, with more crowded spots). The density of 73 had shown that it routes by then.

The final settings are in [src/config.json](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/config.json) and [info.yaml](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/info.yaml) of the chip's repository: `"CTS_APPLY_NDR": "none"`, density 73, hold margin 0.15, and 6 rounds of antenna repair (3 rounds, the default, left one wire too long). What the layout gives:

* The longest path takes 13.5 ns in the typical corner and 26.1 ns in the slowest one (the 20 ns target is missed there by 6.1 ns), so the chip runs at 74 MHz or at 38 MHz at most in those corners; the bench runs it at 12.5 MHz, a period of 80 ns.
* 53,786 cells, 921,326 um of wire, 234 antenna diodes.
* Design rules, the comparison of the layout with the circuit, the antenna rules, and the power grid: no error. 2 pins are not connected, as in the earlier layouts.
* The slew of 8,946 pins and the capacitance of 183 are above the limits of the cell library in the corners of the analysis; the flow reports this as a warning, as it did for every layout before.

Checked on the gates of this layout: `make gates` (the chip test with eight seeds, the start on both versions of the demo board), `make gates-programs` (27 programs with two seeds each, the kernel with its program `array`), the cocotb test that GitHub runs, and the program of the official precheck, all 17 checks of its newest version (7d2d2ef of 2026-11-23). The precheck ran with Magic 8.3.623 and KLayout 0.30.7 of the layout container instead of its own container, because Docker's disk could not take that container's image; the pinned versions of the official action are KLayout 0.30.8 and the Magic of its Nix pin. The x86 build of the tools, as GitHub runs it, makes the same layout.

## How It Is Checked

Five assembly programs for the rules (`cpu_array`, `cpu_learn`, `cpu_sat`, `cpu_array_user`, `cpu_dbg_lock`), new checks in `cpu_excpt_invalid`, 4,096 random rounds against an independent model in Python (`long/array_random`), and the program `array` under the kernel, where two processes use the array while the timer switches between them. 25 mistakes planted in the emulator and 11 planted in the kernel were all caught ([../asc2-tests/Coverage.md](../asc2-tests/Coverage.md)). The hardware runs the same programs. On the simulated chip every instruction is compared with the emulator, also with slow and lost answers of the RP2040, with a debug read after every instruction (70,090 reads in the kernel's own tests), and on the chip's own time. 72 mistakes were planted in the hardware description of the array as it was first built, with cells for every row, of the bit A, of the lock, and of the new bits of the debug port: 71 were caught, and the one left changes nothing (it adds the rows in the other order). 33 more were planted in the array with shared cells, all caught. 16 mistakes were planted in the reset: the simulated start caught 15, and the one left changes nothing. On the board the suite of `make board-suite` passed, with `board-lock` (the port is open before the program and locked after it) and `board-regs` (the debug port compared with the emulator after every instruction).

## Open

* The one-bit helper (`popcnt`) is the first thing dropped if the layout runs short (decided 2026-11-25).
* The kernel does not lock the debug port by itself. Whether a shipped system locks it at its start is a choice of whoever builds that system; the chip offers the lock.
* The stepper on the OLED writes "the debug port is locked" for a locked chip. No test reads that screen; it was not seen by eye either.

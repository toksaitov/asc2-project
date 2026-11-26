The Design of the CPU Core (2026-11-22)
=======================================

This note records how the hardware description of the Almurut II CPU is built and why. The rules that it implements are in the comments of [asc2-emulated/asc.h](../asc2-emulated/asc.h); the note does not repeat them. It was written before the code, as the outline of phase 5 of the plan of milestone 1, and corrected afterwards where the code came out differently.

## The Shape

The core is a multi-cycle state machine, as in Almurut I and as the multi-cycle MIPS of Patterson and Hennessy. Every memory access goes over the bus to the RP2040 and takes hundreds of clocks, so clocks inside the chip are free and area is not. One instruction is done before the next one starts. A shift moves one bit per clock, `mul` adds and shifts for 32 clocks, and there is one adder.

The files, in [src/](https://github.com/toksaitov/ttsky26e-almurut2/tree/main/src) of the chip's repository:

* [asc_cpu.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_cpu.sv): the state machine, the operand registers, the control registers, the address checks, and the trap entry.
* [asc_decoder.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_decoder.sv): tells whether a word is an instruction, and whether the instruction is privileged.
* [asc_regs.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_regs.sv): the register file, 12 words with one read port and one write port.
* [asc_alu.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_alu.sv): the arithmetic and logic unit, without registers.
* [asc_tlb.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_tlb.sv): the eight translations with their comparators.
* [asc_dot.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_dot.sv) and [asc_dot_cell.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_dot_cell.sv): the array of the dot products, a row of four cells and the rows of weights and inputs that go through them.
* [asc_pic.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_pic.sv), [asc_pit.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_pit.sv), [asc_kbdc.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_kbdc.sv), [asc_ps2.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_ps2.sv): the interrupt controller, the timer, the keyboard controller, and the receiver of the keyboard's two wires.
* [asc_soc.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_soc.sv): the CPU with its devices, the divider of the timer's clock, and the debug port.

There is no package: the Tiny Tapeout port check reads the sources with a Yosys that has no package import. Each file holds the constants that it needs as `localparam`.

## The Registers of the Datapath

* `ir_q`: the instruction word.
* `a_q`: the first operand, register A or the immediate value.
* `b_q`: the second operand, the value of the destination register. A shift happens in it, and `mul` takes the bits of the multiplier from it.
* `alu_q`: the output of the ALU, kept for a clock: the result on its way to a register, or the address of a data access. In `mul` it collects the product.
* `cnt_q`: the steps left of a shift, of `mul`, of `popcnt`, or of the array of the dot products.
* `ip_q`, `flags_q`, and the control registers. `cr6`, the stack pointer of user mode, is word 11 of the register file; `sp` names word 9 in kernel mode and word 11 in user mode.

The ALU takes its first input from `b_q`, `a_q`, `ip_q`, `alu_q`, `alu_q` shifted left by one, or zero, and its second from `a_q`, the immediate value, the immediate value times 4, the constant 4, all ones, zero, or a value that one instruction alone uses: the byte of a port, a control register, the bit of a condition, the sum of a row of the array of the dot products, a word of that array, or `b_q` held in the range of a byte (`ssat`, `usat`). With nothing else to do the ALU adds zero to `ip`, so a state that raises a trap gets the address to return to from it. A register is written from `alu_q` or from the word that the bus has read, and from nowhere else. The carry and the overflow of an operation wait in two flops next to `alu_q`; zero and sign are taken from `alu_q` in the clock of the write. This keeps the adder and the zero check in two clocks, which matters for the 20 ns of the Tiny Tapeout flow.

## The States

Every instruction goes the same way:

    BOUNDARY  between two instructions: the debug stop, interrupts, a halted CPU
    FETCH     starts the fetch at ip
    DECODE    checks the word and the privilege; a_q <= first operand
    READ_B    b_q <= second operand
    EXEC      the step of the instruction; more states for a shift, mul, popcnt, the array of the dot products, a call, a pop
    WRITE     the destination register and the flags
    RETIRE    ip <= ip + 4 unless the instruction has set ip itself

A memory access, whether a fetch or data, goes through three shared states:

    XLATE      the checks of the address in their order, the TLB
    BUS_START  hands a request to the bus unit and holds it until the unit has taken it
    BUS_WAIT   waits for the answer and decides what follows

A register tells what the bus request is for: the access itself, the entry of the top page table, the entry of the second table, or the address of a handler. After a missing translation the two entries are read through the same two bus states, the TLB is loaded, and the access starts again at XLATE, where it now finds the translation. The page of the second table and the two permission bits of the top entry wait in flops (`walk_pfn_q`, `walk_w_q`, `walk_u_q`), because the bus unit clears the word that it has read as it takes the next request. An instruction changes no register before WRITE, and a data access is over before WRITE, so an exception finds every register as it was.

## Traps

Any state raises a trap in one clock with a vector, a cause, and the address to continue at. One block writes the trap record, clears U and I, sets T, and sends the CPU to read the handler's address through BUS_START. If T is already set the block sets the double-fault flop and nothing else. The vector is read back from `cr5` for the address of the bus request, so it needs no register of its own. After the handler's address has arrived, an exception goes to BOUNDARY, `int` goes to RETIRE (it counts as a finished instruction), and a device interrupt goes straight to FETCH.

Interrupts are looked at in BOUNDARY only. The interrupt controller clears the request of a line in the clock in which the CPU takes it, and a new request in that clock wins.

## Sequential Fetch

One flop, `fetch_run_q`, follows the rule at `cr0` in [asc.h](../asc2-emulated/asc.h): set after a fetch that was not at the last word of a page, cleared by a trap entry, a write to `ip`, a write to a control register, and a TLB load by the chip's lookup. A fetch asks for "the next word" when the flop is set and F is clear.

## The Hooks for Tests and for the Bench

* `stop`, from the pin `ui_in[7]` through two flops: while it is high the CPU waits in BOUNDARY. The RP2040 drives the clock, so it can lower the pin for a few clocks and let one instruction run. A halted CPU and a CPU after a double fault also sit in BOUNDARY.
* `boundary` leaves the core for the debug port.
* The debug port, three pins: a counter selects one of 643 bits, and the output pin shows that bit. A rise of the port's clock pin selects bit 0 if the data pin is high, and the next bit otherwise. The bits are `ip`, the instruction, `cr5`, `flags`, `cr0`, the state of the CPU, the halted and double-fault flops, the status of the last bus transaction, the state of the bus unit, and `boundary` in the first 128 bits, and then the registers in words of 32 bits: `r0` to `r7`, `fp`, the kernel's `sp`, `lr`, the user's `sp` (`cr6`), and `cr1` to `cr4` (since 2026-11-24, so that a debugger on the RP2040 reads the registers of a stopped or crashed CPU without running an instruction on it); the comment of [asc_soc.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_soc.sv) gives their places. There is no register that takes a copy: the RP2040 holds `stop` high while it reads, so the bits stand still, and 640 flops are saved. The port only reads: the selector is the one addition to the CPU's registers, and no bit of the port can change the CPU. Since 2026-11-25 a kernel can lock the port: a write of the value 0x4c4f434b to `cr8` sets one flop, which only a reset clears. While it is set, the 640 bits read 0 and the bit after them, bit 640, reads 1. The two bits after that one, 641 and 642, show the bits C and A of `cr0`, which are younger than the order of the first 128 bits; they read 0 on a locked port. The lock is one flop, a comparator of 32 bits, and a gate in front of the output; the bus, the clock, the reset, and `stop` do not know of it, so a locked chip is opened by a reset and is never lost.
* The timer steps once every 256 clocks. A test build (`ASC_TEST_TICK` defined, as `ASC_BUS_TEST_TEAR` in the bus unit) adds two inputs to the top module: with the first one high, a pulse on the second steps the timer in place of the 256 clocks. The chip is built without the define and has neither input.

The test program [asc2-tests/chip/asc_trace.cpp](../asc2-tests/chip/asc_trace.cpp) uses both to run the chip in step with the emulator. The emulator moves its devices once per instruction: after an instruction, or after an instruction that has raised an exception, or once per turn of a halted CPU, the timer steps, a byte of the keyboard may arrive, the devices of the RP2040 move, and then a waiting interrupt is taken. The test program holds `stop` high. For one turn it lowers `stop` long enough for the CPU to leave BOUNDARY and waits until the CPU is back there; a CPU that does not leave is halted, and that counts as one turn too. Then it steps the timer once, sends the keyboard's byte as a real frame on the two pins, sets the interrupt line of the RP2040, and waits for the input filters. The test program is built around the top module of the chip and reaches into it for the registers: it prints a line of CPU state after each turn in which the CPU has passed RETIRE.

With the option `-t` the test program uses neither hook. `stop` stays low, the timer steps once in 256 clocks, and the devices of the RP2040 move once per finished instruction, or once in 512 clocks while the CPU is halted. This is the chip as it runs on a board. An interrupt arrives at another instruction than in the emulator, so only the end of a run and its console output are compared.

## The Array of the Dot Products

The rules are in [asc2-emulated/asc.h](../asc2-emulated/asc.h) ("Array of the Dot Products"), and [array-design.md](array-design.md) tells what the array is for and what it gains. This section tells how the hardware does it.

One row of four identical cells, and `ASC_DOT_ROWS` rows (three) of four weights and four inputs, a signed byte each, with no reset. The CPU sends the rows through the cells one after another. A cell holds an 18-bit sum. (Cells for every row were built first and did not fit 16 tiles; [array-design.md](array-design.md) has the sizes.)

For eight clocks the four cells multiply, one bit of a factor per clock from the top bit down; the top bit subtracts, because the bytes have signs. Then the sums move right, one cell per clock: each cell adds the sum of its left neighbor to its own. One adder per cell does both jobs. The ALU adds the sum of the row to a register. The two top bits of `cnt_q` name the row at work, and its three low bits count the steps.

* `dot4` is one pass. A cell takes its byte of the first operand from `a_q`, and its bit of the second from `b_q`, which moves left by one bit per clock, so that the top bit of each of its bytes is the bit in turn. The weights and the inputs take no part.
* `dotv` and `dotm`: a cell multiplies its input by its weight; the count of the steps selects the bit of the weight, so the weights do not move. After the sums have moved right, `b_q` takes a register, the ALU adds the sum of the row, and the register is written; then the next row starts. `dotv` does it three times with one register, `dotm` with three registers in a row. A row takes 14 clocks: eight to multiply, three for the sums to move, three for the register. `dotv` sets the flags at its last write.
* `learn`: `b_q` holds the errors, a byte for a row, and stays as it is; a selector takes the bit of the error of the row at work. A cell multiplies its input by that error, starts the multiplication from its weight where the others start from zero, and adds a 1 in the second step. The seven steps after the first double the weight seven times and the 1 six times, so the sum ends as the weight times 128, plus 64, plus the product. Its bits 14 to 7 are the new weight, rounded to the nearest number; a sum whose bits above 14 differ from bit 14 does not fit a signed byte and gives the nearest end of the range. One more clock writes the weights of the row, and the next row starts: nine clocks for a row. The learning costs a cell eight AND gates and a carry, and the array the check of the range.
* `xsh` moves the inputs by one cell in one clock, and a move to or from a word of the array takes one clock too.
* `popcnt` is not in the array: `b_q` takes the register and moves left for 32 clocks, and the ALU adds its top bit to `alu_q` through the carry input. `ssat` and `usat` are a check of the bits above bit 6 of `b_q` and a selector.

The bit A of `cr0` is checked in DECODE with the privilege of the instruction: an instruction that names a word of the array traps there in user mode while A is clear, before anything has changed.

## What Was Left Out on Purpose

* No second read port of the register file, no bypass, no pipeline register beyond `alu_q`.
* No barrel shifter and no multiplier array.
* No instruction that removes one translation.
* The registers of the datapath (`a_q`, `b_q`, `alu_q`, `ir_q`) have no reset: each is written before it is read. Everything that a program or the trace can see is reset.
* No register that holds a copy of the debug port's bits.

## What the Review of the Chip Changed

A review of the chip on 2026-11-22, after the first layout runs, found no instruction that works wrongly. Its eight findings, with what was done:

1. The reset (marked as a must). Two parts. The registers of the bus unit get their reset values only on the 11th clock of a reset, so until then the request line and the drivers of the bus can have any level, and an RP2040 that listens already could answer a request that nobody made. Rule 5 of [bus-protocol-v1.md](bus-protocol-v1.md) now asks for 20 clocks of reset, during which the RP2040 neither drives the bus nor listens. The second part: the filter of the reset counted clocks in a counter that nothing resets. A real flop starts with a 0 or a 1, and the counter then works. A simulator that starts every flop as unknown never gets a known value out of it, so in Icarus Verilog, which the tests of the Tiny Tapeout template use, the chip never left its reset. The filter now keeps the last eight samples and has no counter. [asc2-targets/tiny-tapeout/tb/tt_um_reset_tb.sv](../asc2-targets/tiny-tapeout/tb/tt_um_reset_tb.sv) checks it, on the Verilog of `sv2v` and on the gates.
2. A request after four lost answers. The bus unit ends such a transaction without a change of the sequence bit, so the next request has the same bit, and the RP2040 takes a request that is equal byte for byte for a repeat and sends its kept answer. No change: the request that follows four lost answers is always the read of a handler's address, and it equals the lost request only if that one was the same read of the same word, which has the same answer. A lost fetch or write has another header, and a lost read of a handler's address ends in the double-fault stop.
3. The time limit of the keyboard receiver, 128 steps of 256 clocks, is 2.6 ms at 12.5 MHz, long enough for a broken frame to be joined with the next byte. To do in pass two, with the tests of broken frames: a limit of 16 steps.
4. STOP does not hold the CPU between the entry of an interrupt and the first instruction of its handler, because the entry goes straight to the fetch. To do in pass two, with the work on the debug port (point 7).
5. The eight lines of the bus have one sampling flop. No change: the chip takes a byte only after the answer line has held still for several clocks, and by the rule of the bus the byte is on the lines before the answer line changes. The four-hour run on the bench was clean with this. The byte that the bus unit loads after the last answer, which can show on the pins for a clock before the drivers turn off, is harmless and stays.
6. The timing reports: the names of the nets with slow edges, the clock tree, and the pins with the delays of a board were not looked at one by one. To do in pass two. Known already: the hold margin of 0.105 ns is what is left after the 0.25 ns of uncertainty that the tools subtract.
7. The debug port shows bits that can change while they are read, shows an instruction word without a value before the first fetch, and does not show the second fault of a double fault. A register that takes a copy of the first 128 bits on one clock would cost about 3,000 um2, which the chip now has room for. Work for pass two.
8. A write of the timer's high byte on the clock of a tick: the chip reloads with the old byte, the emulator with the new one, and the rule in [asc.h](../asc2-emulated/asc.h) names the low byte only. To do in pass two with the other events on the same clock: the chip forwards the new byte.

The review also read the state machine, the translations, and the order of events against [asc.h](../asc2-emulated/asc.h) and found them equal. It chose 6x2 tiles and the clock of 40 ns. Later layout runs showed that the area is the same at 20 ns and the chip twice as fast, so [config.json](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/config.json) asks for 20 ns again; [tt-flow.md](tt-flow.md), section 14, has the numbers.

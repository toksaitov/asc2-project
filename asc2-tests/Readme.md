Almurut II Tests
================

This directory contains the tests of the machine, of the C compiler, and of the kernel. The overview of the whole platform is in [../Readme.md](../Readme.md).

* [asm/](asm/): assembly programs that check themselves. Each one explains a part of the machine in its comments and then checks it.
* [long/](long/): assembly programs for `dot4` and for the array of the dot products that run for millions of instructions and print sums, with the sums that they must print.
* [c/](c/): C programs that print their results. Each one is built for the machine and for the computer that runs the tests, and the two outputs must be equal.
* [gain/](gain/): the measurement of what the cache and the array of the dot products save.
* [kernel/](kernel/): the sessions of the kernel in [../asc2-rach2/](../asc2-rach2/): the lines that are typed to it, and what the console must show.
* [board/](board/): what runs only on the board: a program for the disk in the flash of the Pico, one for `dot4`, one for the physical addresses above 16 MB, one for the keyboard, and one for the lock of the debug port, with their outputs, and the output of the smoke program.
* [bus/](bus/): the test of the chip's bus unit in SystemVerilog, against a model of the RP2040. It has its own [Makefile](bus/Makefile) and is not a part of `make` here.
* [chip/](chip/): the whole chip in simulation, with a program in the place of the RP2040 that serves the bus from the memory and the devices of the emulator. It runs the programs of [asm/](asm/) and [c/](c/) and the kernel on the chip and compares every instruction with the emulator. It has its own [Makefile](chip/Makefile) too.
* [Makefile](Makefile): builds the programs and runs them on the emulator.
* [Coverage.md](Coverage.md): the list of what each program checks.

## Running

```bash
make
```

This runs the four suites. Each prints one line for every program that has passed and stops at the first one that has failed.

* `make emu-suite` builds the assembler and the runner if needed, assembles every program of [asm/](asm/) into `build/emu/`, and runs it.
* `make long-suite` runs the four programs of [long/](long/) and compares what they print with the `.expect` files. [long/array_expect.py](long/array_expect.py) computes the total of `array_random` in Python, by the rules of the instructions of the array. [long/dot4_expect.py](long/dot4_expect.py) computes the sums of two of them once more in Python, by the rule of the instruction and nothing else, and the suite compares that too.
* `make c-suite` builds the C compiler in [../asc2-tcc/](../asc2-tcc/) if needed, then builds and runs every program of [c/](c/) in `build/c/`.
* `make kernel-suite` builds the kernel and its programs in [../asc2-rach2/](../asc2-rach2/), boots the kernel seven times with a disk and typed lines, and compares what the console shows with the files of [kernel/](kernel/). The session `screen` starts `scenes` and the game `burkut` from the shell with the presses of [kernel/screen.events](kernel/screen.events): the last picture of the scenes must have the pixels of the bare machine's ([kernel/has_frame.py](kernel/has_frame.py)), and at the end the screen must show the console again ([kernel/screen.frame](kernel/screen.frame)). `make board-suite` runs the same session on the board.

`make measure` is not a test. It runs the program `load` under the kernel with three ways to choose the length of a turn and prints what the scheduler has counted ([../asc2-kb/sched-network.md](../asc2-kb/sched-network.md)).

`make gain` is not a test either. It writes six small jobs in assembly (a neuron of a layer, a small network, a filter, a matrix times a vertex, a neuron of one-bit inputs, a step of learning), each with the instructions that the chip had before the array of the dot products and with those of the array, runs them without and with the instruction cache, and prints the requests on the bus that one unit of work takes ([gain/gain.py](gain/gain.py), [../asc2-kb/array-design.md](../asc2-kb/array-design.md)). Every value that a job stores is compared with a model in Python.

`make board-gain` measures the time of the same jobs on the board, and `make gain` in [chip/](chip/) their clocks on the simulated chip ([gain/gain_time.py](gain/gain_time.py)). On the board a program reads the clock of the bridge, in microseconds, at its start and at its end. On the simulated chip the test counts the clocks while the chip runs on its own time, for an RP2040 that answers at once and for one that answers after 4 clocks, as the bridge of the bench does at 12.5 MHz. Every program first runs on the emulator, where its values are compared with the model.

`make board-suite` is not one of the four. It runs the smoke program of [../asc2-firmware/](../asc2-firmware/), a program for the disk of the Pico ([board/disk.s](board/disk.s)), a program for the instruction `dot4` ([board/dot4.s](board/dot4.s)), a program for the physical addresses above 16 MB ([board/high.s](board/high.s)), the three programs of [long/](long/), the C programs, and the three sessions of the kernel on the board: the chip in a DE10-Lite and a Pico with the firmware of [../asc2-bridge/](../asc2-bridge/), both on the table and loaded. Each program must print what it prints on the emulator; [board/hello.expect](board/hello.expect), [board/disk.expect](board/disk.expect), and [board/dot4.expect](board/dot4.expect) are the outputs of the first three. A line of `passed` gives the number of bus transactions and of those that broke off and were repeated. For the kernel the target writes the disk image into the flash of the Pico with `picotool`, reads the disk back after the sessions, and checks it as the kernel suite checks the disk of the emulator. One C program also starts with its CPU held, runs its first 40 instructions one at a step, each compared with the emulator's trace, and is stopped and let go once more in the middle of its run. The target takes about ten minutes, most of them for the session `tests`.

`make board-keys` checks the keyboard on the board. The Pico sends three bytes as a keyboard does, and [board/keys.s](board/keys.s) must print them; [board/keys.events](board/keys.events) gives the emulator the same bytes, and [board/keys.expect](board/keys.expect) is the output. `make board-suite` runs it first of all. It needs the two wires of the keyboard, from GP16 and GP17 of the Pico to the two keyboard pins of the board.

`make board-lock` checks the lock of the debug port on the board. [board/lock.s](board/lock.s) prints a line, locks the port, and prints what `cr8` reads. The bridge reads the port while the CPU is held before its first instruction, where it must be open, and after the last line, where it must show the lock and no register. `make board-suite` runs it before `make board-regs`, which reads the registers of another program through the port after a reset. Both need the three wires of the debug port.

`make board-button` and `make board-screen` check the push button and the screen, which are devices of the RP2040. In the first, [board/button.s](board/button.s) sleeps, the Pico plays two presses, and the program must count two interrupts and two presses; the emulator runs it first with the presses of [board/button.events](board/button.events). The second runs the scenes of [../asc2-demos/](../asc2-demos/) and compares the frame that the Pico has sent to the display with the frame of the emulator, pixel for pixel. Neither needs a hand on the button or an eye on the display. [asm/rp_screen.s](asm/rp_screen.s) and [asm/rp_button.s](asm/rp_button.s) check the registers of both on the emulator and on the simulated chip.

## How a Program Reports Its Result

The runner is `../asc2-tools/reftrace/asc_reftrace`, the emulator without a screen. It runs a program until the CPU is parked: a `hlt` has run, and no interrupt can wake the CPU.

Every suite runs it with `-r`, which fills the RAM around the variables of a program with the byte `0xff`. The RAM of the real machine is not zero after a reset, so a program that counts on zeros there must fail here too.

A program keeps the number of its current check in `r7`. When a check fails, the program copies `r7` to `r0` and parks the CPU. When all checks have passed, it parks the CPU with `0x600d` in `r0`.

```asm
    li 0x3 r7
    li 0x5 r0
    add 0x2 r0
    cmp 0x7 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt
```

The pass code is not zero. Empty memory is full of zeros, and a zero is the instruction `hlt`, so a program that jumps into empty memory by mistake parks the CPU too.

The runner ends with one of these statuses:

* 0: the program has passed.
* 1: the program has failed. The runner prints the number of the check.
* 2: the runner could not run the program.
* 3: the CPU has stopped with a double fault.
* 4: the program has not parked the CPU within 100000 instructions. The C suite and the kernel suite raise the limit with `-n`.

The programs in `EMU_DOUBLE_FAULT_TESTS` of [asm/tests.mk](asm/tests.mk) pass with the status 3 instead of 0. They check the double fault itself, so a CPU that stops is their right end.

## Files Next to a Program

A program may come with files of the same name:

* `.events`: the bytes that the keyboard sends, each with the number of instructions that the program has finished before the byte arrives.
* `.in`: the bytes that the console receives.
* `.expect`: what the program must print on the console.
* `.disk`: the first bytes of the disk. The [Makefile](Makefile) adds four sectors of zeros after them.
* `.bus`: every request that the CPU must send on the bus, one per line, as the runner writes them with `-b`. [../asc2-tools/reftrace/trace_common.h](../asc2-tools/reftrace/trace_common.h) describes the columns.
* `.stop`: the line that the runner must print when the CPU stops with a double fault. The line has the trap record in it.

After a change that changes the requests or the stop on purpose, read the new file in `build/emu/` line by line before it replaces the one in [asm/](asm/). A file taken over unread would hide the very change that the test is there to catch.

## C Programs

A C program of [c/](c/) is linked from these parts:

* [start.s](c/start.s): the first instructions. They set the stack pointer, set the variables without a starting value to zero, call `main`, and park the CPU with the pass code if `main` has returned 0.
* The program itself, `name.c`.
* [out.c](c/out.c): functions that print text and numbers, built on `put_char`.
* [console.s](c/console.s): `put_char`, which writes a byte to the console of the RP2040.
* The runtime library of the compiler.

The same `name.c` and [out.c](c/out.c) are built with the C compiler of this computer, with [host.c](c/host.c) as `put_char`. The output of that program is what the machine must print.

A program may have a part in assembly, `name_asm.s`. Then `name_host.c` holds the same functions in C for the build on this computer.

When a program fails, `build/c/name.expect` has the output of this computer and `build/c/name.out` the output of the machine:

```bash
diff build/c/name.expect build/c/name.out
```

The two builds agree only where C leaves no choice, so a program follows these rules:

* It uses `char`, `short`, `int`, and `long long`, with `signed` or `unsigned` where the sign of a `char` matters. `long` and pointers have other sizes on this computer: their sizes are not printed, and they are not fields of a structure whose size or layout is printed.
* It does nothing that C leaves undefined: no overflow of a signed number, no shift by 32 or more or by a negative count, no division by zero, no two changes of one variable in one expression.
* Where the behavior of the machine is its own (a division by zero, memory at a fixed address), the code is under `#ifdef __asc2__`, and the other branch prints what the machine must print.

## The Kernel

The kernel starts the shell, and the shell reads the console. A session is a file of typed lines, `kernel/name.in`, which the runner gives the console with `-i`. The kernel echoes what it reads, so the console output of a session, `kernel/name.expect`, reads like the screen of a terminal. The disk is `../asc2-rach2/build/tests/disk.img`: the shell, the utilities, and the test programs of [../asc2-rach2/tests/](../asc2-rach2/tests/). The runner saves the disk that a run leaves with `-s`.

The suite has seven sessions:

* `shell`: the shell and the seven utilities, with redirection, wrong names, a line that is typed into a file, and a line that is corrected with Backspace. Its two files hold the bytes of `ctrl+d` and of Backspace and spaces at the ends of lines, all on purpose: an editor that tidies a file must leave them alone. The run must end with the pass code. `../asc2-tools/krffs/asc_krffs` then takes the copy of `ish` that the session has made out of the disk, and the copy must have the bytes of `ish`.
* `again`: a second boot, from the disk that `shell` has left. `cat` must print the files of the first session. `ls` must print what `asc_krffs list` prints for that disk: the lines of a listing start with a space, and the suite compares them with the list and the other lines with [kernel/again.expect](kernel/again.expect). The sizes of the programs change with every change of their code, and so they are in no file here. The last command of the session ends with the exit code 3, and `exit` passes it on: the kernel must print it and stop without the pass code.
* `tests`: the programs `work`, `crash`, `forks`, `execs`, `files`, `dots`, `array`, `load`, and `screen`. Each prints one line for every part of the kernel that it has found right. The disk that they leave must have the files that it had.
* `edit`: the line editor `ed` makes a file, changes it, writes it, and reads it again.
* `screen`: `scenes` and the game `burkut` from the shell, with presses of the button.
* `showcase`: the programs of the showcase, from the disk of the machine itself (`../asc2-rach2/build/disk.img`; the disk of the tests has no room for them). The presses of [kernel/showcase.events](kernel/showcase.events) take each one through its pictures and end it with a hold; the benchmark `mark` ends the session.
* `tour`: the program `tour` with no press at all: it starts the programs of the showcase one after another, and each one shows itself and ends.

What the last two sessions print depends on when a press reaches a program, and so on how long the programs before it took to load. After a change of the disk or of the kernel the lines differ; when the difference is what the programs should say, copy `build/kernel/showcase.out` and [showcase.frame](kernel/showcase.frame) (or `tour.out` and [tour.frame](kernel/tour.frame)) over the files of [kernel/](kernel/). After every session `asc_krffs check` must find the file system of the disk sound.

The line before the last one says that the scheduler's network got the same sum from `dot4` and from C at every turn; the kernel stops with a message at the first sum that differs. The last line of a session tells the least number of free pages that the kernel has seen. In `again` it is the cost of a command: the shell, its copy, and the program together leave 26 of the 48 pages of RAM free.

When a session fails, the files of the run are in `build/kernel/`:

```bash
diff kernel/shell.expect build/kernel/shell.out
```

The output must not depend on how many instructions fit between two timer interrupts, because that number differs on the chip and on the board. A test program that needs an order between two processes makes it itself: with `waitpid`, or with a file that one process makes and the other waits for. After a change of the kernel that changes the output on purpose, read the new output in `build/kernel/` line by line before it replaces the file in [kernel/](kernel/).

## The Bus Unit

```bash
cd bus/
make
```

This needs Verilator 5 and Yosys. [asc_bus_tb.sv](bus/asc_bus_tb.sv) runs the bus unit of the chip and its two input filters ([asc_bus.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_bus.sv), [asc_reset_filter.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_reset_filter.sv), and [asc_irq_filter.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_irq_filter.sv), from the clone of the chip's repository beside this one) at their pins against a model of the RP2040 that follows [../asc2-kb/bus-protocol-v1.md](../asc2-kb/bus-protocol-v1.md). Its header comment lists what it checks; the last line of a run is `PASS`.

* `make lint`: Verilator's lint on the three files.
* `make sim`: the test with a short time-out of the bus unit, so that a fault is over quickly.
* `make tear`: the test with the input that throws the bus unit back to idle in the middle of a transaction.
* `make mutants`: five broken copies of the files; the test must fail on each.
* `make gates`: the test on the gates that Yosys makes from the files.
* `make long`: `sim` and `gates` with the time-out of the chip, 65535 clocks. It takes minutes.

The top module of the chip has a test of its pins in [../asc2-targets/tiny-tapeout/](../asc2-targets/tiny-tapeout/), and the same test for cocotb is in the chip's repository, in [test/](https://github.com/toksaitov/ttsky26e-almurut2/tree/main/test). Both serve the chip the same program of six instructions and check its pins.

## The Chip

```bash
cd chip/
make
```

This needs Verilator 5. [asc_trace.cpp](chip/asc_trace.cpp) runs a firmware image on the hardware description of the chip, as `asc_reftrace` runs it on the emulator, and prints what `asc_reftrace` prints. The chip is the Tiny Tapeout top module, with the files of its repository read from the clone beside this one ([../chip.mk](../chip.mk)). The program stands in for the RP2040 at its pins: it drives the clock and the reset, answers the requests on the bus as [../asc2-kb/bus-protocol-v1.md](../asc2-kb/bus-protocol-v1.md) asks of the RP2040, sends the bytes of the keyboard as frames on its two pins, and holds the interrupt line. The memory and the devices that it answers from are the emulator's own code, so a firmware image meets the same machine in both tools.

* `make suite` runs every program of [asm/](asm/) on the emulator and on the chip. The two runs must end the same way and write the same lines of CPU state, the same requests on the bus, the same console output, and the same last words.
* `make slow` repeats three programs with an RP2040 that takes 500 clocks to answer.
* `make faults` repeats two programs with every fifth answer held back once.
* `make debug` repeats every program of [asm/](asm/) and the kernel's session `tests` with the debug port read at its three pins, after every seventh instruction of a program and after every 499th of the kernel. All 640 bits are read with the CPU held, and `ip`, the twelve registers, and `cr1` to `cr4` must be the values inside the chip. The run must also stay the run of the emulator, so a read changes nothing.
* `make programs` runs the C programs and the kernel in the same way. The kernel runs its sessions `shell` and `tests`, and the two runs must also leave the same disk. The programs run for millions of instructions, so only a checksum of their lines of CPU state is kept. This target takes about eight minutes.
* `make own-time` runs all of these programs once more on a chip that is left alone, as on a board. This target takes about six minutes.

The emulator moves its devices once per instruction, and the chip has a clock. The program keeps the two in step: it holds the CPU between two instructions with the STOP pin, lets one instruction run, and then gives the devices their time, which is one step of the timer, the byte of the keyboard if one is due, and the level of the interrupt line. [../asc2-kb/cpu-design.md](../asc2-kb/cpu-design.md) says more under The Hooks for Tests and for the Bench.

In `make own-time` the program does not keep the two in step. The STOP pin stays low, the timer steps once in 256 clocks, and the devices of the RP2040 move once per finished instruction. An interrupt then arrives at another instruction than in the emulator, so the lines of CPU state and the requests on the bus differ, and the target compares what must not: the way the run ends, the console output, and the last words.

```bash
build/obj/asc_trace [-n max_retired] [-c max_clocks] [-q] [-r] [-t] [-w answer_delay] [-f fault_period] [-g debug_period] [-e event_file] [-i console_input_file] [-o console_output_file] [-d disk_file] [-s disk_output_file] [-b bus_log_file] path_to_binary
```

* `-w` makes the RP2040 slow: it lets that many clocks pass before it answers a change of the request line.
* `-f` holds back the answer to the first attempt at every transaction with that number. The chip waits, stays quiet, and sends the transaction again. The RP2040 answers a second time and does not repeat the access.
* `-g` reads the debug port at its pins after every instruction with that period and compares it with the registers of the chip.
* `-c` is the number of clocks after which the program gives up, four billion if not given.
* `-t` leaves the chip on its own time, as `make own-time` does, and prints no lines of CPU state.
* The other options are the ones of `asc_reftrace`.

The program stops with an error when the chip breaks a rule of the bus: both sides drive the bus, the chip does not leave the bus a clock before it hands it over, the header of a request is not valid, or the chip repeats a transaction that was answered. When a program fails, the files of the two runs are in `build/emu/` and `build/chip/`:

```bash
diff build/emu/name.trace build/chip/name.trace | head
```

The comment of `trace_print_row` in [../asc2-tools/reftrace/trace_common.h](../asc2-tools/reftrace/trace_common.h) names the columns of a line. The first line that differs shows where the two runs part.

## Adding a Program

For an assembly program:

* Start the file with a comment that explains the part of the machine under test, as the existing programs do.
* Number the checks in `r7` and end as shown above. A program that turns interrupts on turns them off with `cli` before its last `hlt`.
* Do not count on exact times. The timer and the devices of the RP2040 run at other speeds on the chip, so wait in a loop with a limit for what must happen.
* Add the name to `EMU_TESTS` in [asm/tests.mk](asm/tests.mk) and a line to [Coverage.md](Coverage.md). The program then runs on the emulator and, with `make` in [chip/](chip/), on the chip.
* Break the emulator in the place that the new check guards, see the program fail, and restore the emulator.

For a C program:

* Start the file with a comment that explains how C uses the machine for the feature under test.
* Print every result with the functions of [out.h](c/out.h), and return 0 from `main`.
* Add the name to `C_TESTS` in the [Makefile](Makefile) and a line to [Coverage.md](Coverage.md).
* Break the code generator or the runtime library in the place that the new check guards, see the program fail, and restore it.

For a check of the kernel:

* Add it to the program of [../asc2-rach2/tests/](../asc2-rach2/tests/) that it belongs to, with one line of output when it is right: `check` of [tests/check.h](../asc2-rach2/tests/check.h) prints the line, or the same line after `FAILED`. A new program gets a line in `TESTS` of the [Makefile](Makefile) there and in [kernel/tests.in](kernel/tests.in).
* A check of the shell or of a utility is a line of [kernel/shell.in](kernel/shell.in).
* Add the new lines to the `.expect` file and a line to [Coverage.md](Coverage.md).
* Break the kernel in the place that the new check guards, see the suite fail, and restore the kernel.

Almurut II Test Coverage
========================

This file lists what the programs in [asm/](asm/) and [c/](c/) and the boot of the kernel check, so that a gap shows during review. The programs run on the emulator and, in [chip/](chip/), on the hardware description of the chip; The Chip below says what the second run adds.

## Instructions

| Instructions                                           | Programs                                      |
| ------------------------------------------------------ | --------------------------------------------- |
| `li`, `lih`, `mov`, `push`, `pop`                      | `cpu_data_move`                               |
| `lr`, `lrh`, `lrb`, `lrhu`, `lrbu`, `sr`, `srh`, `srb` | `cpu_data_move`                               |
| `add`, `adc`, `sub`, `sbb`, `cmp` and their flags      | `cpu_add_sub`                                 |
| `and`, `test`, `or`, `xor`, `not` and their flags      | `cpu_bitwise`                                 |
| `shl`, `shr`, `sar` and their flags                    | `cpu_shift`                                   |
| `mul`, `dot4`                                          | `cpu_mul_dot4`                                |
| `dot4` alone, see "The Dot-Product Unit"               | `cpu_dot4`, `long/dot4_*`                     |
| `dotv`, `dotm`, `xsh`, moves of `w0`-`w2`, `x0`-`x2`   | `cpu_array`, `long/array_random`              |
| `learn`                                                | `cpu_learn`, `long/array_random`              |
| `ssat`, `usat`, `popcnt`                               | `cpu_sat`, `long/array_random`                |
| the ten conditional jumps to a label and to a register | `cpu_branch`                                  |
| the ten `set` instructions                             | `cpu_branch`                                  |
| `jmp` to a label, backward, and through a register     | `cpu_branch`                                  |
| `call`, `ret`, the calling convention, recursion       | `cpu_call_ret`                                |
| `call` through `lr`                                    | `cpu_call_ret`                                |
| instruction words encoded by hand                      | `cpu_call_ret`                                |
| `ip` and `flags` as operands                           | `cpu_reg_special`                             |
| `flags` as the destination of an addition              | `cpu_reg_special`                             |
| `int`, `iret`                                          | `cpu_privilege`, `cpu_nested_trap`            |
| `cli`, `sti`, `hlt` and the wake-up from it            | `cpu_irq_pit`                                 |
| `inb`, `outb`                                          | `cpu_irq_pit`, `cpu_irq_kbd`                  |
| moves of the control registers                         | `cpu_privilege`, `cpu_nested_trap`, `mmu_tlb` |

## Exceptions

| Behavior                                                     | Programs                                                |
| ------------------------------------------------------------ | ------------------------------------------------------- |
| invalid opcode, register number, and unused field (vector 0) | `cpu_excpt_invalid`                                     |
| privileged instructions in user mode (vector 1)              | `cpu_privilege`                                         |
| misaligned read, write, stack access, and fetch (vector 2)   | `cpu_misaligned`                                        |
| page fault (vector 3)                                        | `mmu_lookup`, `mmu_fault`, `mmu_user_fault`             |
| translation miss (vector 4)                                  | `mmu_soft_fill`                                         |
| bus error inside the CPU and from the RP2040 (vector 5)      | `cpu_bus_error`                                         |
| `int` with its number in the cause (vector 6)                | `cpu_privilege`, `cpu_nested_trap`                      |
| the trap record in `cr1`, `cr2`, `cr4`, `cr5`                | every program above                                     |
| an instruction that raises an exception changes nothing      | `cpu_excpt_invalid`, `cpu_misaligned`, `mmu_user_fault` |
| a handler that saves the record and takes a second trap      | `cpu_nested_trap`                                       |
| a second exception with the record not saved stops the CPU   | `cpu_double_fault`                                      |
| the record of the first exception stays after the stop       | `cpu_double_fault`, `rp_lost_vector`                    |
| a bad instruction word in user mode is vector 0, not 1       | `cpu_privilege`                                         |
| `iret` to an address that the new mode cannot fetch from     | `cpu_privilege`                                         |

## Kernel Mode and User Mode

| Behavior                                                                       | Programs                                        |
| ------------------------------------------------------------------------------ | ----------------------------------------------- |
| entering user mode with `iret` and leaving it with `int`                       | `cpu_privilege`                                 |
| a user program cannot change I or U in `flags`                                 | `cpu_privilege`                                 |
| the two stack pointers and `cr6`                                               | `cpu_privilege`                                 |
| the addresses from `0x80000000` and the device registers refuse a user program | `cpu_privilege`                                 |
| a user program with paging on                                                  | `mmu_lookup`, `mmu_user_fault`, `mmu_soft_fill` |

## Paging

| Behavior                                                                        | Programs         |
| ------------------------------------------------------------------------------- | ---------------- |
| translation through two tables; the P, W, and U bits; the cause of a page fault | `mmu_lookup`     |
| the kernel honours W; the kernel window with paging on                          | `mmu_lookup`     |
| malformed entries, tables where no memory is                                    | `mmu_fault`      |
| tables in the boot ROM and in the device registers are refused unread           | `mmu_fault`      |
| permission is needed in both tables; ignored bits                               | `mmu_fault`      |
| a failed lookup leaves nothing behind                                           | `mmu_fault`      |
| remembered translations outlive a changed entry; `cr3` and `cr0` forget them    | `mmu_tlb`        |
| 8 remembered translations and the order of replacement                          | `mmu_tlb`        |
| `cr7`: load, replace, ignored values, reads zero                                | `mmu_tlb`        |
| a user program that gets its pages on the first access and is resumed           | `mmu_user_fault` |
| the kernel reads the tables instead of the CPU, with both permissions           | `mmu_soft_fill`  |

## The Bus

| Behavior                                                                           | Programs         |
| ---------------------------------------------------------------------------------- | ---------------- |
| which fetches carry an address: jumps, traps, control registers, the end of a page | `cpu_fetch_next` |
| the F bit of `cr0`, set by a reset                                                 | `cpu_fetch_next` |
| pages that do not follow each other in memory; `cr3`, `cr7`, `cr0`; a lookup       | `mmu_fetch_next` |
| every request on the bus, in order, against a list                                 | both of them     |
| a fetch from a device register is refused with the status 2                        | `cpu_bus_error`  |
| a lost answer: a read, a write, a fetch, `push`, `pop`, the page table entries     | `rp_lost_answer` |
| a translation stays after a lost answer                                            | `rp_lost_answer` |
| a lost answer on the way into a handler                                            | `rp_lost_vector` |

## Devices on the Chip

| Behavior                                                              | Programs      |
| --------------------------------------------------------------------- | ------------- |
| interrupt controller: mask, first vector, a request kept while masked | `cpu_irq_pit` |
| timer: period, restart, counter, interrupt on line 0                  | `cpu_irq_pit` |
| interrupts wait while I is clear; the handler runs with I clear       | `cpu_irq_pit` |
| keyboard controller: data, status, interrupt on line 1                | `cpu_irq_kbd` |
| keyboard controller: the lost-byte bit stays until it is written      | `cpu_irq_kbd` |
| two interrupts waiting at once: the lower line goes first             | `cpu_irq_kbd` |
| only the low byte of a port number counts; a port with no device      | `cpu_irq_pit` |

## Devices of the RP2040

| Behavior                                                             | Programs         |
| -------------------------------------------------------------------- | ---------------- |
| identification, scratch, and clock registers                         | `rp_console`     |
| console output and input, by asking and by interrupt on line 2       | `rp_console`     |
| the interrupt line is a level                                        | `rp_console`     |
| registers take 4-byte accesses only                                  | `cpu_bus_error`  |
| disk: read and write of a sector, by asking and by interrupt         | `rp_disk`        |
| disk: bad sector, bad buffer, registers ignored while a command runs | `rp_disk`        |
| sleeping for the disk with the timer as a guard                      | `rp_disk`        |
| the register that orders a lost answer                               | `rp_lost_answer` |

## The C Compiler

The programs in [c/](c/) check the code generator, the linker, and the runtime library of [../asc2-tcc/](../asc2-tcc/). The output of each on the emulator must equal the output of the same C file built for the computer that runs the tests.

| What C Does                                                                    | Programs         |
| ------------------------------------------------------------------------------ | ---------------- |
| calls from C to assembly and from assembly to C, by name and through a pointer | `calls`          |
| the order and the size of arguments; 64-bit arguments and results              | `calls`, `llong` |
| a result shorter than 32 bits comes back extended                              | `calls`, `types` |
| `r4`-`r7` kept across a call; the arguments removed after a call               | `calls`          |
| variables and addresses shared by an assembly file and a C file                | `calls`          |
| recursion, calls as arguments, more arguments than registers                   | `calls`          |
| bytes and 16-bit numbers with and without a sign: loads, stores, conversions   | `types`          |
| global variables with and without a starting value, `static` variables         | `types`          |
| pointers, arrays, addresses in data, an array longer than 64 KB                | `types`          |
| unions, enumerations, `_Bool`, bit-fields                                      | `types`          |
| variable argument lists, with 64-bit arguments, passed on to a function        | `varargs`        |
| 64-bit carry and borrow, product, division, shifts, comparisons                | `llong`          |
| 32-bit arithmetic, division and remainder, shifts                              | `ops`            |
| the ten comparisons as numbers and as jumps                                    | `ops`            |
| constants at the limits of the 16-bit field of an instruction                  | `ops`            |
| expressions that need more values at once than there are registers             | `ops`            |
| `if`, loops, `switch`, `goto`, the logical and the conditional operators       | `ops`            |
| large local arrays; arrays of a size found at run time                         | `ops`            |
| division by zero in the runtime library                                        | `ops`            |
| layout and copies of structures; structures as arguments and results           | `structs`        |
| tables of structures with addresses of strings and functions                   | `structs`        |
| `memset`, `memcpy`, `memmove` at every alignment and with overlap              | `mem`            |
| zeros in the parts of a local array or structure without a starting value      | `mem`            |
| memory at a fixed address, as for the registers of a device                    | `fixed`          |

The linker joins the object files of the assembler and of the compiler in every one of these programs, so they test the object files of the assembler too: its symbols and its four relocation types. A mistake in either tool shows as a wrong line of output.

## The Kernel

`make kernel-suite` boots the kernel of [../asc2-rach2/](../asc2-rach2/) three times, with a disk and with typed lines, and compares the console output with the files of [kernel/](kernel/). `shell`, `again`, and `tests` are the sessions; `work`, `crash`, `forks`, `execs`, `args`, and `files` are the programs of [../asc2-rach2/tests/](../asc2-rach2/tests/), which the session `tests` runs.

| What the Kernel Does                                                                                                                                                 | Shown By                  |
| -------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------- |
| the amount of RAM from the register of the RP2040; the count of free pages                                                                                           | the first line            |
| the size of the disk, found by asking for sectors; the file system on it; `ish` started from its file                                                                | every session             |
| a program loaded from its ELF file: code on two pages, variables with and without a starting value                                                                   | `work`: variables         |
| a new page of the stack on its first touch, full of zeros, in RAM that was not zero                                                                                  | `work`: stack             |
| `r1`-`r7`, `fp`, and `lr` of a process kept through timer interrupts and a system call                                                                               | `work`: registers         |
| a wrong address, descriptor, or call number answered with its error                                                                                                  | `work`: refusals          |
| the kernel reads and writes a buffer of a program across two pages, and adds them if they are new                                                                    | `work`: the border        |
| the timer takes the CPU from a process that does not give it up; two processes take turns                                                                            | `work`: the last lines    |
| a write to the address 0 or into the code, a call of the kernel, an instruction for the kernel only, and an unknown `int` stop the process, and its parent learns it | `crash`                   |
| a child of `fork` has a copy of the memory and of the open files, and its own number                                                                                 | `forks`, `files`          |
| `exit` and `waitpid`: the exit code, a wait for one child and for any child, no wait for a stranger                                                                  | `forks`                   |
| `fork` fails when RAM is full, and every page comes back: a second chain is as long as the first                                                                     | `forks`                   |
| a process ends before its children, and their entries are freed                                                                                                      | `forks`                   |
| `execve` refuses a missing file, a name that is too long, the directory, a wrong address, and too many arguments                                                     | `execs`                   |
| 17 damaged ELF files are refused, and the process goes on                                                                                                            | `execs`                   |
| the arguments and the environment of a new program: empty strings, none, and as many as the kernel takes                                                             | `execs`, `args`           |
| a file keeps its bytes over three sectors; reads and writes at any position; `lseek`                                                                                 | `files`                   |
| the flags of `open`; wrong requests refused; no ninth descriptor                                                                                                     | `files`                   |
| `dup2`; a file that moves on the disk while it is open; `rename` and `unlink`                                                                                        | `files`                   |
| `stat`, `getdents64`, `chdir`, and `utimensat`                                                                                                                       | `files`                   |
| two processes write to the disk at the same time, and each file gets its bytes                                                                                       | `files`                   |
| a disk without room: a write takes the bytes that fit or fails, and the room comes back                                                                              | `files`                   |
| a typed line is echoed and given to the reader; Backspace takes a byte back; `ctrl+d` ends the input                                                                 | `shell`: `cat > f.txt`    |
| the shell: commands with arguments, `<` and `>` in any order, a name that is no program, `cd`, empty lines                                                           | `shell`                   |
| `echo`, `cat`, `ls`, `cp`, `mv`, `rm`, and `touch`, with their messages for wrong names                                                                              | `shell`                   |
| a copy of a file of two sectors has the bytes of the file                                                                                                            | the copy of `ish`         |
| the disk keeps its files over a reset of the machine                                                                                                                 | `again`                   |
| `ls` lists the files that are on the disk                                                                                                                            | `again`, `asc_krffs list` |
| the exit code of a command reaches the shell, and `exit` passes it on                                                                                                | `again`: the code 3       |
| the exit code of the first process alone decides the result of the machine                                                                                           | the status of the runs    |
| the file system is sound after every session                                                                                                                         | `asc_krffs check`         |
| the test programs leave the disk with the files that it had                                                                                                          | the lists of `tests`      |
| every page of an exited process comes back                                                                                                                           | the stop of the kernel    |
| the count of repaired page faults; the least number of free pages                                                                                                    | the last line             |

The sessions run with `-r`, so every page that the kernel gives out held the byte `0xff` or the bytes of an earlier process before the kernel wrote to it. The kernel stops with a message if a page is missing after the last process has exited.

`asc_krffs` reads the format from the same header as the kernel and has its own code for everything else. The disk of a session also passed the strict check of Rach2's own tool, `fsck.krffs`, by hand.

## The Bus Unit and the Pins

[bus/asc_bus_tb.sv](bus/asc_bus_tb.sv) checks the bus unit and the two input filters at their pins, in simulation, against a model of the RP2040. They were also run on the bench with a DE10-Lite and a Pico before they came here.

| Behavior                                                                                | Shown By        |
| --------------------------------------------------------------------------------------- | --------------- |
| every transaction arrives as issued and its answer comes back; each access happens once | every phase     |
| the chip and the RP2040 never drive the bus together; REQ changes only inside FRAME     | every phase     |
| noise pulses on ACK, the reset, and the interrupt line are ignored                      | phase 1         |
| an RP2040 that answers late, wrongly, or not at all costs one retry and no wrong value  | phases 2 and 4  |
| a state register upset by a bad clock edge, also back to idle, costs one retry          | phase 3, `tear` |
| a dead RP2040 ends every transaction with the link error, never a hang                  | phase 5         |
| a reset in the middle of a transaction leaves a working bus                             | phase 6         |
| the same on the gates that Yosys makes                                                  | `make gates`    |
| five planted faults make the test fail                                                  | `make mutants`  |

[../asc2-targets/tiny-tapeout/tb/tt_um_tb.sv](../asc2-targets/tiny-tapeout/tb/tt_um_tb.sv) checks the chip as it is built, without the inputs that tests add, and on its own clock:

| Behavior                                                                               | Shown By |
| -------------------------------------------------------------------------------------- | -------- |
| during the reset the chip drives no pin of the bus, and the free outputs keep a level  | step 1   |
| the STOP pin holds the CPU before its first instruction                                | step 2   |
| the CPU fetches from the boot ROM, writes a word to RAM, and halts                     | step 3   |
| the sequence bit of the header changes with every transaction                          | step 3   |
| a short pulse on the interrupt line starts nothing; a level wakes the halted CPU       | step 4   |
| the debug port shows `ip`, the instruction, `cr5`, `flags`, `cr0`, the halted CPU, and the registers | step 5   |

Twelve planted mistakes in the top module, the debug port, and the CPU made it fail. The same test passes on the gates that the official layout run made of the whole chip (`make gates` there), with eight different sets of values in the flip-flops at the start, and on the Verilog that `sv2v` makes of the sources (`make sv2v`). Two wires crossed in a copy of the gates made it fail.

Whole programs run on the gates of a layout with `make gates-programs` there: 27 programs of [asm/](asm/), each from two sets of values in the flip-flops, must send the requests of the emulator on the bus, one by one, and the kernel must boot and run the program `array` from its shell with the console output of the emulator. One gate changed in a cell of the array, in a copy of the gates, made `cpu_learn` fail, and a kernel run that was cut short made the second part fail.

[tb/tt_um_powerup_tb.sv](../asc2-targets/tiny-tapeout/tb/tt_um_powerup_tb.sv) copies the start on the demo board of Tiny Tapeout (`make powerup` there; the Readme of the target tells what it copies and why):

| Behavior                                                                                         | Shown By |
| ------------------------------------------------------------------------------------------------ | -------- |
| a chip that was selected and clocked without a reset has every pin at its level after 11 clocks of reset | step 2   |
| the reset released while the clock stands, as the rules of the firmware ask, starts the chip     | step 2   |
| the STOP pin holds the CPU, and the debug port is open and shows the reset address                | step 3   |
| the reset button inside a request, with ACK high, makes the chip begin anew with its first fetch  | step 4   |
| the reset button while the RP2350 gives an answer does the same                                   | step 5   |
| the chip drives the bus no sooner than 10 clocks after the release of the reset (it takes 15)     | step 5   |
| the chip and the RP2350 never drive the bus together                                              | every step |

It runs for both revisions of the board (a reset pin that shows many edges for one, and one clean edge at any moment), for free pins of the bus that read low, high, and by chance, and with four sets of values in the flip-flops at the start: 24 runs. 15 planted mistakes of the reset made it fail, and one more changes nothing; the Readme of the target lists them. `make gates` there runs it on the gates of a layout run.

[tb/tt_um_reset_tb.sv](../asc2-targets/tiny-tapeout/tb/tt_um_reset_tb.sv) runs with both targets in Icarus Verilog, where every flip-flop starts as unknown: after 12 clocks of reset every output pin has its level, and after the reset the chip asks for its first instruction. It failed on the gates of an earlier filter of the reset, which a simulator of this kind never got out of the unknown state.

## The Chip

[chip/asc_trace.cpp](chip/asc_trace.cpp) runs the top module against the memory and the devices of the emulator. Every program of [asm/](asm/), every program of [c/](c/), and the kernel run on the chip and on the emulator, and the two runs are compared: the registers, `ip`, `flags`, and `cr0` to `cr5` after every instruction, every request on the bus, the console output, and the way the run ends. A row of the tables above so holds for the chip too, except for what Not Covered lists.

| Behavior                                                                          | Shown By        |
| --------------------------------------------------------------------------------- | --------------- |
| the chip does what the emulator does, instruction by instruction                   | `make suite`    |
| the same for the C programs and for the kernel with its shell and its tests       | `make programs` |
| the same with an RP2040 that takes 500 clocks to answer                           | `make slow`     |
| an answer held back once is asked for again, and the access happens once          | `make faults`   |
| the debug port shows at its pins the registers that the chip holds                | `make debug`    |
| the bytes of the keyboard arrive as frames on its two pins                        | `cpu_irq_kbd`   |
| the chip and the RP2040 never drive the bus together                              | every run       |
| every program ends the same way and prints the same on a chip that is left alone  | `make own-time` |

The timer of the chip steps once in 256 clocks. In the first four runs the program steps it by hand, once per instruction, as the emulator does, and holds the CPU between two instructions meanwhile. In `make own-time` it does neither: the timer runs on the chip's clock, and an interrupt arrives whenever it does. The kernel runs its sessions `shell` and `tests` so, that timer interrupts the work of its processes, and the console shows what the files of [kernel/](kernel/) hold. In `make programs` the two runs of a session must also leave the same disk, byte for byte.

Before a run the flip-flops of the chip hold random values, from the seed 1 unless `ASC_SEED` in the environment names another. The program does not look at the bus in the first 11 clocks of the reset, as the firmware of the Pico does not: the registers have their reset values by then. Without this rule a layout of the gates showed a request at its pins in the fourth clock of the reset, in the middle of random state, and the runs on the gates failed for two seeds of three. `make suite` passes with the seeds 1, 2, 3, and 7.

## The Board and the Bridge

[../asc2-targets/de10-lite/tb/asc2_tb.sv](../asc2-targets/de10-lite/tb/asc2_tb.sv) checks in simulation what the top module of the DE10-Lite adds around the chip, against a small model of the Pico:

| Behavior                                                                            | Shown By    |
| ----------------------------------------------------------------------------------- | ----------- |
| during the reset nobody drives the wires of the bus, and the LED of the reset is on | step 1      |
| the slide switch of STOP holds the CPU before its first instruction                 | step 2      |
| the wire of STOP from the Pico holds it in the same way                             | step 2      |
| the CPU fetches, writes, and halts over the wires that the FPGA and the Pico share  | step 3      |
| STOP raised when the header of a fetch has come lets that one instruction run       | step 3      |
| while the Pico says that it drives, the FPGA's byte is off the wires                | step 4      |
| the LEDs follow their wires                                                         | step 5      |
| each of the two wires of the keyboard reaches its pin of the chip                   | step 6      |
| all of it with each of the two clocks that a slide switch chooses, the other still  | both passes |

Seventeen planted mistakes in the top module made the test or the lint fail: no guard, drivers that stay on during the reset, two LEDs swapped, the interrupt line and ACK swapped, STOP from another switch, the clock always from the board, always from the Pico, the switch the other way, a dark LED for the clock, a divider that stands still, STOP from the switch alone, from the Pico alone, and from both together only, the two wires of the keyboard swapped, each of them tied high, and an LED of STOP that shows the switch alone. [../asc2-bridge/firmware/test/devices_test.c](../asc2-bridge/firmware/test/devices_test.c) checks the memory and the devices that the firmware of the Pico serves ([devices.h](../asc2-bridge/firmware/devices.h)) against the emulator's model of the RP2040. Both get the same four million random requests, with typing on the console, disk commands, and resets between them:

| Behavior                                                                               | Shown By               |
| -------------------------------------------------------------------------------------- | ---------------------- |
| every answer and every value read equal the emulator's                                 | every request          |
| the pointer of "the next word", the interrupt line, and the console output are equal   | every request          |
| a reset leaves the devices as the emulator's reset does                                | 16 resets              |
| RAM and the disk are equal at the end                                                  | the end                |
| a request of 8 bytes, a misaligned one, and "the next word" for data are refused       | the bridge's own cases |
| a console output queue that is full drops the new byte and counts it                   | the bridge's own cases |
| every kind of answer was met: done, error, protection, no next word, a lost answer     | the guard at the end   |

49 planted mistakes in [devices.h](../asc2-bridge/firmware/devices.h) made it fail, and the compiler refused two more. One changes nothing: a pointer of "the next word" past the end of memory is never valid in either model.

`make board-suite` needs the two boards. The smoke program of [../asc2-firmware/](../asc2-firmware/), the eight C programs, and the three sessions of the kernel run on the chip in the FPGA with the firmware on the Pico, and each must print what it prints on the emulator. The disk of the kernel is in the flash of the Pico: the session `again` boots from what `shell` has left there, and the disk is read back after `again` and after `tests` and must be sound and have the files that the emulator's disk has. [board/disk.s](board/disk.s) checks the disk of the Pico, which is a part of its flash: it writes a sector of numbers that differ from run to run, reads it back, sleeps until the end of a command interrupts, and is refused a sector that the disk does not have. The target was seen to fail on a program that had not finished within its time, and on a firmware that reads a sector from the wrong place. It does not run the assembly programs of [asm/](asm/): they leave their result in a register, which the board cannot show before the debug port has its wires.

The same target checks the commands `stop`, `step`, and `go` of the bridge's monitor. The C program `types` starts with its CPU held and must send nothing. Its first 40 instructions then run one at a step, and each must be the instruction of the emulator's trace at the address of the trace. The program is let go, stopped in the middle of its run, where the chip must send nothing for a fifth of a second, stepped once, and let go again, and it must print what it prints without all that. Four planted mistakes made the check fail on the board: a bus server that leaves STOP low at the fetch of a step (40 steps ran more than 40 instructions), a `stop` and a `run stopped` that leave the pin low, and a changed line of the trace.

[board/dot4.s](board/dot4.s) runs the instruction `dot4` on the board: seven pairs of words, with mixed signs, the largest and the smallest sum, and a sum that wraps around. It prints every sum, and it computes each one a second time with `mul` and stops if the two differ. The sums of [board/dot4.expect](board/dot4.expect) were worked out by hand. A changed sum in the expected output and a program whose own sum leaves out one product both made the check fail on the board.

`make board-keys` is the check of the keyboard on the board: the Pico sends three bytes as a PS/2 keyboard does, and [board/keys.s](board/keys.s) prints what the keyboard controller got. The emulator runs the program first with the same bytes. `make board-suite` runs it first of all. Before the bench had the two wires of the keyboard it failed, as it must: the program waited for a byte that did not come. With the wires it passes, and a byte that differs from the expected one makes it fail.

`make board-button` and `make board-screen` are the checks of the push button and of the screen on the board (2026-11-24). The button: two presses played by the Pico, two interrupts and a count of two in the program. The screen: the last frame that the Pico sent to the display is equal to the emulator's. Not covered: that the glass of the display shows the frame (the display cannot answer), and a press by a finger with its bounce; both are checked by hand. [asm/rp_screen.s](asm/rp_screen.s) and [asm/rp_button.s](asm/rp_button.s) cover every register of the two devices, the modes of rectangles and pictures, pictures partly outside the screen, and the interrupt flag, on the emulator and on the simulated chip; the firmware's copy of the devices gets them in its four million random requests.

## The Dot-Product Unit

`dot4` is checked on the emulator, on the chip in simulation (every instruction compared with the emulator), and on the board, by these programs:

* `cpu_mul_dot4`: the rules of the instruction, a few sums worked out by hand.
* `cpu_dot4`: one bit at a time in every place of both words, against the bytes 1, 3, 5, and 7; 47 pairs of words with the signs, the largest and smallest bytes, and bytes next to the border of two places, each in both orders; the destination the same register as one source, as the other, and as both; `fp`, `lr`, `sp`, `flags`, and `ip` as operands; two `dot4` in a row, and `dot4` next to `mul` and to shifts by 0, 1, and 31, which use the same counter and the same shift register in the chip; user mode with its own stack pointer.
* `long/dot4_lanes`: every pair of bytes, all 65,536, in each of the four places, each product compared with `mul`.
* `long/dot4_random`: 16,384 pairs of random words, each sum added to a random number. The program prints the sum of all results, and [long/dot4_expect.py](long/dot4_expect.py) computes the same number in Python.
* `long/dot4_irq`: 4,096 pairs while the timer interrupts every 37 instructions and the handler runs a `dot4` of its own, so an interrupt arrives right before and right after a `dot4` many times, with the unit used in between.
* `dots` of the kernel's tests: two processes compute sums with `dot4` in user mode with paging on, the timer switches between them, and each sum is compared with four multiplications of C.
* The kernel itself: the scheduler computes a `dot4` before every turn of a process and the same sum in C, and stops the machine if they differ ([../asc2-rach2/sys/proc/slice.c](../asc2-rach2/sys/proc/slice.c)). Every session of the kernel is such a check, several hundred sums on live values.
* [board/dot4.s](board/dot4.s): seven sums on the board, printed and compared.

Ten mistakes were planted in the chip's description of the unit, one at a time, and the five assembly programs were run on each wrong chip in simulation: the first step adds where it must subtract; the first step does not start from zero; a byte of the first word is not taken as signed; the count of the steps is one short; the first step comes a clock late; the three sums are passed down the chain all at once; they are passed in the wrong order; the destination is read from the wrong register; two bytes of the second word are swapped; the sum is not taken as signed when it is added. Every one of the five programs failed on every one of them. A mistake planted in the emulator (one more in the product of two certain bytes in the fourth place) made `long/dot4_random` and `dots` fail; `long/dot4_lanes` passes it, because it compares the emulator with its own `mul` one place at a time, which is why the random program has its reference in Python.

## The Array of the Dot Products

The instructions of the array are checked on the emulator by these programs:

* `cpu_array`: the six registers with a word each; one bit of a weight at a time, all 96, against inputs that differ from cell to cell, and one bit of an input at a time against such weights; 54 entries for `dotv` with the largest and smallest sums, one cell at a time, all cells but one, sums of zero, a sum that passes the largest positive number, hard bytes, and random words, each over flags that were all set, with the registers of the array compared after it; 20 entries for `dotm` over clear and set flags; `dotm` from `r0` and from `r5`; `sp` and `flags` as the sum of `dotv`; `xsh` through all three rows, with the dropped bytes, the upper bytes of its register, and a byte with its top bit set; `dotv` and `dotm` next to `dot4`, `mul`, and a shift.
* `cpu_learn`: 37 entries with an error of zero, products next to a half of a step on both sides, the errors -128 and 127, weights at the ends of the range, a fourth byte of the errors, an error of its own for each row, hard bytes, and random words, over clear and set flags, with the inputs compared after it; steps up to the end of the range; a neuron of twelve inputs that learns its target in four steps; `learn` next to `mul`, `dot4`, and a shift, and `dotv` with the new weights.
* `cpu_sat`: 32 values for `ssat`, `usat`, and `popcnt` around both ends of both ranges, over clear and set flags; one bit at a time; `sp` as an operand; a sum of a network of one bit a weight; the three next to `mul`, `dot4`, and a shift.
* `cpu_array_user`: with the A bit of `cr0` clear, a user program gets eight privileged instruction exceptions from the eight uses of the array and changes nothing; `dot4`, `popcnt`, `ssat`, and `usat` work; a row that the array does not have and a `dotm` past `r7` are invalid instructions, not privileged ones; with A set the user program uses every instruction; with A clear again they are privileged again, and the kernel goes on using the array.
* `cpu_excpt_invalid`: the registers `w3` and `x3`, the register of the array with the number 8, `dotm r6`, and fields that the instructions of the array do not use.
* `long/array_random`: 4,096 rounds of random words through every instruction. The program prints one total of all results and flags, and [long/array_expect.py](long/array_expect.py) computes the same number in Python.
* `array` of the kernel's tests: two processes use the array in user mode with paging on, the timer switches between them, and every sum of `dotv` and, every eight rounds, every register is compared with a model in C; a program starts with zeros, also when the process that starts it has its values in the chip; a child has the values of its parent, whether the chip holds them or the kernel; what a child writes is its own.

25 mistakes were planted in the emulator's rules, one at a time, and the program that guards the rule failed on each: `dotv` without its last row, with C left as it was, with an input taken as unsigned; `dotm` with one register for all rows, with a flag changed; `xsh` that passes no byte to the next row, that takes the whole register; a move to an input that writes a weight; `learn` without the half, with a division that rounds toward zero, without the upper end, without the lower end, with one error for all rows; `ssat` that holds at 128; `usat` that lets -1 through; `popcnt` without the top bit; the A bit ignored, the array privileged with A set, `xsh` not covered by A; a row that the array does not have taken as valid (two programs); `dotm r6` taken as valid; a lock of the debug port that any value closes, that the low half of the key closes, that a second write opens. Eleven mistakes were planted in the kernel's handling of the registers ([../asc2-rach2/sys/proc/array.c](../asc2-rach2/sys/proc/array.c)), one at a time, and `array` failed on each: the registers of the old owner are not stored; the values of the process are not loaded; the new owner is not remembered; the switch is on for every process; the switch stays on for a process that is not the owner; a program that starts keeps the values of its record; a program that the owner starts keeps the values of the chip; a process that ends stays the owner; `fork` does not take the newest values from the chip; `fork` does not copy the values; a mistake of the owner gets the array again (the kernel then never stops the program, and the session runs out of time).

The chip is checked against the emulator by the same programs. `make suite` in [chip/](chip/) runs the assembly programs on the hardware description next to the emulator: the registers after every instruction, every request on the bus, the console, and the way the run ends must be equal. `make debug` there reads the debug port at its pins after every seventh instruction. `cpu_dbg_lock` is read through an open port while the bits C and A of `cr0` are set, and through the locked port, which must show the lock and nothing else, also while A is set. `make programs` runs `long/array_random` and the kernel's sessions, with `array` among them, in step with the emulator.

On the board, `make board-suite` runs `long/array_random` and the kernel's `array`, and `make board-lock` runs [board/lock.s](board/lock.s): the bridge reads the debug port before the first instruction, where it must be open, and after the last line, where it must show the lock and no register. The program that the suite loads next is stepped with its registers read through the port, so the reset has opened it. A program that prints the same lines and does not lock made `board-lock` fail.

72 mistakes were planted in the hardware description of the array as it was first built, with cells for every row, of the bit A, and of the lock, one at a time, and the programs ran on each wrong chip:

* `learn`: a start from zero in place of the weight; no half; the weight taken as unsigned at the start; the half one step late; no upper end; no lower end; a check of the range that is one bit short; the error of the first row for every row; no write of the weights; the errors from the other register.
* The multiplication: the bits of the weights in the wrong order, in the first row and in the others; `dot4` with the inputs of the array, and with its weights and inputs; one step too few.
* The registers: a move to the weights that writes the inputs too, and the other way; `xsh` that passes no byte from a row to the next; `xsh` that moves the weights too; two bytes swapped at a read; the weights read where the inputs are named; a move to the array that names its register by the wrong field.
* The sums: the last cell of a row left out; `dotm` with one register for all rows; `dotv` with a register for each row; the last row left out; `dotv` without its flags; `dotm` with flags; C and O left as they were; a sum taken as unsigned; the rows added in the opposite order.
* The bit A: ignored; the array refused with A set; a clear A that refuses the kernel too; A the other way round; A lost at a write of `cr0`; A set after a reset; `xsh`, `learn`, `dotv`, `dotm`, and each of the two moves not covered by A; `ssat` and `usat` covered by it.
* The decoder: a row that the array does not have taken as valid, as a source and as a destination; a code above 7 that names a register of the array; `dotm` from a register whose last row has none; `dotv` held to the limit of `dotm`.
* `popcnt`, `ssat`, `usat`: no count of the top bit; one step too few; the bits of the other register; a count that doubles in every step; `ssat` that holds at 128; `usat` that lets a negative value through; `ssat` that makes a negative value zero; `ssat` that holds -128 back.
* The lock: closed by any value; closed by the low half of the key; opened by a second write; `cr8` that reads zero; a key that never locks; a locked port that still shows the registers; one that does not say that it is locked; one that hides the first word only; an open port that says that it is locked.
* The bits C and A of `cr0` on the debug port: no A; no C; the two in each other's place; shown by a locked port; a port that ends before them; the bits T and F in their place.

61 of them failed in `make suite` and 10 in `make debug`. The one left changes nothing: the rows added in the opposite order give the same sums in the same registers. The lint refuses six of the 71 before a test runs; these six were run with the warnings of the lint allowed. Two of the ten showed gaps first: no program was read through a locked port with a bit of `cr0` set, or through an open port with C set. `cpu_dbg_lock` got both cases.

The array was then built anew with one row of cells that the rows go through one after another. 33 mistakes were planted in the new description of the array and of its control, one at a time, and `make suite` failed on each:

* The row at work: the weights, the inputs, or both always of the first row; the row forgotten after the multiplication; `learn` and the two sums that stop after the first row or leave the last row out; the second row done twice; the next row started at its second step.
* `learn`: the bits of `dot4` in place of the error; the weight in place of the error; no start from the weight; a new weight written to every row, or to the first row alone; the error of the first row for every row; the bits of the error from the low one up; errors that move away while the cells multiply; the half one step late; no end of the range, and the two ends exchanged.
* The sums and the cells: the sum of the first cell added by every cell; the sum of the third cell taken for the last; every cell that adds at the first chance; the third cell that adds with the second; the second step taken for the first; `dot4` with the inputs of the array; a second factor of `dot4` that does not move.
* The registers: `xsh` that gives every cell the new byte; a move to the weights that writes the inputs too; the weights read where the inputs are named; `dotm` with one register for all rows; `dotv` without its flags.

The lint refuses four of the 33 as a signal without a use. These were planted again in a form that the lint accepts.

## The Lock of the Debug Port

* `cpu_dbg_lock`: `cr8` reads 0 after a reset; a user program can neither read it nor lock the port; 0, 1, -1, and the key with each of its 32 bits wrong change nothing; the key locks; no later write opens. What the port shows while it is locked is checked on the chip.

## The Checks Themselves

Every check was shown to fail once: about 90 single mistakes were put into a copy of [asc.h](../asc2-emulated/asc.h), one at a time, and the suite was run on each. The suite noticed all of them except one that no program can see (where the next translation goes after the CPU has forgotten all of them).

The rules that the review of the contract added were tried in the same way: 40 single mistakes in [asc.h](../asc2-emulated/asc.h), among them one for every event that ends a run of sequential fetches. The suite noticed 39. The one left changes the lines of CPU state that the runner prints (`int` without a line of its own); [chip/](chip/) compares those lines with the chip's.

The C programs were tried in the same way: 136 single mistakes in [asc2-gen.c](../asc2-tcc/asc2-gen.c), [asc2-link.c](../asc2-tcc/asc2-link.c), and the runtime library, which was one C file then, one at a time, each followed by a rebuild of the compiler and a run of the C suite. The suite noticed 133. The other three change no program: a `_Bool` reaches the code generator as a byte, a function called through a pointer always has its address in `r0`, and `memset` may leave its last whole word to its loop over bytes.

The kernel of the first boot, without a disk, was tried in the same way: 90 single mistakes in its C and assembly files, one at a time, each followed by a rebuild and a run of the kernel suite. The suite noticed 80. Checks were added on the way for the ones first missed, and two pieces of code that no check could reach were removed. The ten that are still out of reach are listed under Not Covered.

The kernel with the disk and the shell has not had such a pass yet. Each kind of check of its suite was seen to fail once, on seven planted mistakes: a wider column of `ls`, a `cp` that shifts the bytes of a whole buffer, an `ls` that skips an entry, a shell that forgets the exit code, a test that leaves a file, a kernel that writes one of its two cached sectors only, and a console that knows one of the two codes of Backspace only.

The chip was tried in the same way: 203 single mistakes in the files of the core of the chip (its CPU and devices, in [src/](https://github.com/toksaitov/ttsky26e-almurut2/tree/main/src) of its repository), one at a time, each followed by a rebuild and `make suite` in [chip/](chip/). The suite noticed 187. The other 16 are listed under Not Covered. `make programs` was seen to fail on one of the 187.

The program of [chip/](chip/) was tried too: 16 single mistakes in [asc_trace.cpp](chip/asc_trace.cpp), among them the order of the bytes of an address and of an answer, the sequence bit of the status, a missing handback of the bus, a repeated request served as a new one, ACK left unequal to REQ after a silent attempt, the registers read from the wrong words, no step of the timer, and a wrong parity bit of the keyboard. `make` in [chip/](chip/) noticed all of them.

`make own-time` was seen to fail on a divider of the timer's clock that never gives a step. `make suite` does not notice that mistake, because it steps the timer by hand.

## Not Covered

* A broken frame of the keyboard. The emulator takes whole bytes, and [chip/](chip/) sends only good frames. Four mistakes in the receiver went unnoticed: no check of the start bit, of the parity bit, or of the stop bit, and no time limit for a frame that stops in the middle.
* Exact times: the period of the timer in clock cycles and the duration of a disk command differ between the emulator and the chip, and no program depends on them. `rp_disk` counts on a command that is still running six accesses after its start and has ended 40,000 instructions later.
* Two events on the same clock. The emulator moves the devices once per instruction, and [chip/](chip/) moves them while the CPU waits, so neither can place a new request on the clock that delivers an older one, a byte of the keyboard on the clock of a read, or a write of the timer's period on the clock of a tick. [asc.h](../asc2-emulated/asc.h) says what the hardware does ("Order of Events"). Three mistakes in the chip went unnoticed, one for each of the three.
* The registers of a chip on its own time. `make own-time` compares how a program ends and what it prints. Its interrupts arrive at other instructions than in the emulator, so nothing compares the registers after each instruction in such a run, and a mistake that these programs do not turn into a wrong result or a wrong line on the console stays unseen.
* The stepper's words for a locked debug port. The firmware writes "the debug port is locked" on the display in place of the registers. The command `frame` of the bridge shows the console's frame and not the stepper's, so no test reads these words; they are for the eye.
* An opcode that is not in the list. `cpu_excpt_invalid` tries some of them; a chip that took `0xa9`, a `not` with an immediate value, as an instruction went unnoticed.
* A misaligned access of user mode to the upper half of the addresses. The alignment is checked first; a chip that reported the bus error went unnoticed.
* A translation loaded for a page that the TLB already holds. A chip that kept the old one beside the new one went unnoticed.
* Bit 23 of `cr5`, which reads as zero after any write.
* In the interrupt controller: a byte of the keyboard that arrives while line 1 is masked (the timer's line is checked), the mask of line 2, and a data byte after a command that is neither 0 nor 1. The same data byte in the timer.
* In the keyboard controller: the interrupt of a byte that arrives while another one waits.
* The delays of the gates. `make gates` and `make gates-programs` of the Tiny Tapeout target run the gates of a layout without delays; the layout tools check the delays by themselves. On the gates the registers are not compared, only what leaves the pins, and nine programs of [asm/](asm/) do not run there: those that need typed keys, a disk, presses, lost answers, the steps of the timer, or the display, and the one that ends with a double fault.
* An answer that is lost once and arrives on the second try. The CPU sees nothing of it; [bus/asc_bus_tb.sv](bus/asc_bus_tb.sv) and [chip/](chip/) check it at the pins.
* The status 4 (a fetch of "the next word" that the RP2040 cannot continue). A CPU that follows the rule never asks so.
* A keyboard byte of zero, the same byte twice, and a broken byte followed by a good one.
* What the compiler refuses: floating point, a function with more than 32 KB of local variables, a jump or a call out of reach. The messages were tried by hand.
* `long` and pointers as parts of printed results, because their sizes differ on the computer that runs the tests.
* A program larger than the boot ROM. `asc_elf2fw` refuses it.
* An exception inside the kernel. The kernel makes none in the test, so three mistakes went unnoticed: the bit that protects the trap record not cleared at the entry, the bit not set at the exit, and a page fault of the kernel repaired as if a program had made it. The message and the stop of the machine were not tried.
* The parts that the kernel got with the disk and the shell, one planted mistake at a time. See The Checks Themselves.
* A `fork` that fails because the table of processes is full. RAM is full before the eighth process.
* A disk that fails: a sector that the RP2040 refuses in the middle of a file, and a write that ends with an error.
* A reset in the middle of a system call that changes the disk. The disk is whole between two calls and not inside one.
* A typed line that is longer than the kernel's line.
* `memset` and `memcpy` of the runtime library, which are in assembly since the kernel got the disk, with planted mistakes. The C suite and the kernel suite pass with them.
* `r4`-`r7` across a change of the kernel stack. The compiler does not use them, so two of them in the wrong order in `switch_context` change nothing.
* Two mistakes without an effect: a process that runs but is marked as ready, with one CPU, and a refused access in the range of the stack treated as a missing page, where no page is ever read-only.
* Two rules that the kernel got from the review of the contract: a copy to or from a program needs the permission in both page tables, and the stack grows only for an entry that is absent, not for a malformed one or one that could not be read. Every top entry of the kernel gives all permissions, and no test makes the read of a table fail, so the suite cannot see either.
* The kernel on the screen of `asc` was tried by hand; no suite runs it.
* On the board: a keyboard byte with a wrong parity bit or a lost bit; the registers at a step, which need the wires of the debug port; a step of a program with paging on, an interrupt taken at a step, and a step of a halted CPU, which were tried by hand or not at all.

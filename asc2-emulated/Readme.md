Almurut II Emulator
===================

This directory contains the C emulator of the Almurut II computer. The overview of the whole platform is in [../Readme.md](../Readme.md).

* [asc.h](asc.h) is the machine: the CPU with its instructions, interrupts, exceptions, and paging, and the devices, each written as a small C function. The hardware description must behave as this file does, and its comments are the full description of the machine.
* [asc.c](asc.c) runs the machine in a terminal.

## Building and Running

```bash
make
./asc [-p] [-k] [-d disk_file] path_to_binary
```

* `-p` starts the machine paused.
* `-k` sends the keys to the keyboard controller. Without it the keys go to the console.
* `-d` gives the machine a disk. The file is written back at exit, so it keeps what the machine has written to it.

The binary is a firmware image made by the assembler in [../asc2-assembler/](../asc2-assembler/):

```bash
../asc2-assembler/asa ../asc2-firmware/hello.s ../asc2-firmware/hello
./asc ../asc2-firmware/hello
```

The screen has three panels: the CPU and the devices on the left, the console in the middle, and physical memory on the right. `ctrl+c` exits, and `ctrl+z` pauses or continues. While the machine is paused, `s` runs one instruction, `j` and `k` scroll the memory panel, and `g` and `G` jump to the start of RAM and of the boot ROM.

`asc` is an `ncurses` program and needs a terminal. Tests and scripts use `../asc2-tools/reftrace/asc_reftrace`, which runs the same [asc.h](asc.h) with no screen.

## The Machine

Almurut II is a 32-bit computer in two parts. The chip holds the CPU, the AI accelerator, an interrupt controller, a timer, and a keyboard controller. A microcontroller next to it, the RP2040 of the bench, is its bridge to memory, storage, and peripherals: it answers every memory access over a narrow bus and serves RAM, the boot ROM, a console, a disk, a clock, a screen, and a button. The emulator is both parts.

The sections below are a map. The rules themselves are in the comments of [asc.h](asc.h), and every program in [../asc2-tests/asm/](../asc2-tests/asm/) explains one part of the machine and checks it.

### Registers

* `r0`-`r7`: general-purpose registers.
* `fp`, `sp`, `lr`: the frame pointer, the stack pointer, and the link register, where `call` leaves the return address.
* `ip`: the address of the current instruction. A write to it is a jump.
* `flags`: C (carry), Z (zero), S (sign), O (overflow), I (interrupts are on), and U (user mode).
* `cr0`-`cr8`: the control registers of the kernel.
* `w0`-`w2`, `x0`-`x2`: the weights and the inputs of the array of the dot products.

Kernel mode and user mode have a stack pointer each, and `sp` names the one of the current mode. A user program cannot change I or U, and it cannot use the control registers.

### Instructions

An instruction is one 32-bit word: an 8-bit opcode, two 4-bit register numbers, and a 16-bit immediate value. In assembly the source comes first and the destination last, with no commas:

```asm
    li 0x5 r0
    add r1 r0
    lr 8(fp) r2
    sr r2 -4(sp)
    cmp 0xa r0
    jl loop
```

| Instructions                                                           | Operation                                                                              |
| ---------------------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| `mov`, `li`, `lih`                                                     | copy a register; load a 16-bit value with its sign; replace the upper 16 bits          |
| `lr`, `lrh`, `lrb`, `lrhu`, `lrbu`                                     | load 4, 2, or 1 bytes from a register plus a displacement; `u` loads without the sign  |
| `sr`, `srh`, `srb`                                                     | store 4, 2, or 1 bytes                                                                 |
| `push`, `pop`                                                          | the stack                                                                              |
| `add`, `adc`, `sub`, `sbb`, `cmp`                                      | arithmetic                                                                             |
| `and`, `test`, `or`, `xor`, `not`                                      | logic                                                                                  |
| `shl`, `shr`, `sar`                                                    | shifts                                                                                 |
| `mul`, `dot4`                                                          | the low 32 bits of a product; four products of signed bytes, added to a register       |
| `popcnt`, `ssat`, `usat`                                               | the count of the bits that are set; a value held in -128 to 127, or in 0 to 127       |
| `dotv`, `dotm`, `learn`, `xsh`, moves of `w0`-`w2` and `x0`-`x2`       | the array of the dot products, described below                                         |
| `jmp`, `je`, `jne`, `jl`, `jge`, `jle`, `jg`, `jb`, `jae`, `jbe`, `ja` | jumps to a label or to the address in a register                                       |
| `sete`, `setne`, and the other eight conditions                        | 1 or 0 from a condition                                                                |
| `call`, `ret`                                                          | subroutines                                                                            |
| `int`                                                                  | a call into the kernel                                                                 |
| `hlt`, `cli`, `sti`, `iret`, `inb`, `outb`, moves of `cr0`-`cr8`       | kernel mode only                                                                       |

The source of an arithmetic, logic, or shift instruction is a register or an immediate value. A word that is not an instruction raises an exception. The table of opcodes is `INST_ENC_TABLE` in [asc.h](asc.h).

### The Array of the Dot Products

A neuron multiplies its inputs by its weights and adds the products up. `dot4` does this for four pairs of signed bytes that it finds in two registers. The array does it for twelve pairs that stay on the chip: three rows of four weights and four inputs. The weights of a row are the four bytes of `w0`, `w1`, or `w2`, its inputs the four bytes of `x0`, `x1`, or `x2`, and `mov` copies a word between one of these and a register of the CPU.

| Instruction | Operation                                                                                                          |
| ----------- | ------------------------------------------------------------------------------------------------------------------ |
| `dotv rd`   | adds the twelve products to `rd` and sets the flags by the sum: a neuron of twelve inputs, and its sign for a jump  |
| `dotm rd`   | adds the products of each row to a register of its own, `rd` and the two after it: three neurons, or a matrix times a vector |
| `learn ra`  | adds to every weight its input times the error of its row, a byte of `ra` in 128ths, and holds the weight in a byte  |
| `xsh ra`    | moves every input on by one place and gives the first place the low byte of `ra`: a filter, or the outputs of a layer  |

`ssat` and `usat` turn the sum of a neuron into the signed byte that the next layer takes; `usat` also turns a negative sum into zero, the rule of a rectified linear unit. `popcnt` counts the bits of a word that are set, which is the sum of 32 products in a network of one bit a weight.

A kernel saves the registers of the array for a process that uses them. Bit 5 of `cr0` tells the CPU that it has: while the bit is clear, the instructions of the table and the moves of `w0`-`w2` and `x0`-`x2` raise the privileged instruction exception in user mode. The comment at the array in [asc.h](asc.h) holds the exact rules.

### Addresses

Addresses are 32 bits wide, and so are physical addresses (4 GB). A program sees them in these ranges:

* `0x80000000`-`0xffffffff` is the lower half of physical memory as the kernel sees it, with paging on or off. A user program cannot use these addresses.
* `0x00000000`-`0x7fffffff` is translated through the page tables when paging is on. With paging off, it is the lower half of physical memory.

The upper half of physical memory is reached through a page table only. Physical memory holds RAM at `0x00000000` (192 KB), the boot ROM at `0x00800000` (up to 256 KB), the device registers of the RP2040 at `0x00ff0000`, and 1 KB of RAM at `0xfffff000`, the start of the last page, for tests of addresses above 16 MB. The RP2040 answers every other address with a bus error. The RP2040 keeps both RAM and the boot ROM in its own memory, so a board with another microcontroller has another amount of RAM, and a register of the RP2040 holds the amount. After a reset the CPU is in kernel mode with paging off and runs the boot ROM from `0x80800000`. A 4-byte access needs an address that is a multiple of 4, and a 2-byte access an even one.

### Interrupts and Exceptions

The vector table starts at physical address 0 and holds the 4-byte address of a handler for each vector. The vectors 0-6 are the exceptions: invalid instruction, privileged instruction, misaligned address, page fault, translation miss, bus error, and `int`. A device interrupt on line `n` of the interrupt controller takes the vector 8 + `n`.

The CPU enters a handler in kernel mode with interrupts off, and it writes no memory on the way. It leaves the address to return to in `cr1`, the flags in `cr4`, the cause in `cr5`, and for a memory exception the address in `cr2`. `iret` returns. An instruction that raises an exception has changed no register, so the handler can remove the cause and return to the same instruction.

The record in `cr1`-`cr5` is the only one. While a handler has not saved it, interrupts wait, and a second exception stops the CPU for good (a double fault).

### Paging

A page is 4 KB. `cr3` holds the physical address of the top page table. The CPU reads an entry of the top table and then an entry of a second table, as a 32-bit x86 processor does, and it remembers the last 8 translations. An entry has a present bit, a write bit, a user bit, and the number of a physical page.

Two switches in `cr0` change this. Bit 0 turns paging on. Bit 1 stops the CPU from reading the tables: a translation that it does not remember then raises the translation miss exception, and the kernel hands the translation to the CPU through `cr7`. A page table lies in the first 8 MB of physical memory, where RAM is.

### The Bus

Every instruction and every piece of data comes over the bus from the RP2040, one request at a time. The RP2040 answers a request with a status: 0 if the access was done, or a number that tells why it was not. A status other than 0 raises the bus error exception, or a page fault if the request was for an entry of a page table, and the status is in the bits 26-24 of the cause.

The RP2040 remembers where the last instruction fetch was. While the CPU runs through consecutive instructions it asks for "the next word" and leaves the address out, which makes a fetch a third shorter. Bit 2 of `cr0` turns this off: every fetch then carries its address. A reset sets the bit, so that boot code runs on a chip whose sequential fetch does not work, and the boot code clears it. The comment at `cr0` in [asc.h](asc.h) lists what ends a run of such fetches.

Bit 4 of `cr0` turns the instruction cache on: the chip then keeps the instructions that it has fetched, 32 words in four lines of eight, and a loop that fits runs without the bus. An interrupt, an exception, `int`, `iret`, and a write to a control register empty the cache. The chip does not notice a write to memory, so a program that writes instructions and runs them passes one of these in between.

The status 5 means that the chip got no answer after four tries. The RP2040 may or may not have done the access by then. A register of the RP2040 orders such a loss for a test.

### Devices on the Chip

The devices on the chip use I/O ports, which `inb` and `outb` reach. There are three:

* The interrupt controller (ports `0x20` and `0x21`) has 8 lines, a mask, and a programmable first vector.
* The timer (ports `0x43` and `0x40`) requests an interrupt on line 0 each time it has counted down its period.
* The keyboard controller (ports `0x60` and `0x64`) holds the last byte from the keyboard, tells when a byte was lost, and requests an interrupt on line 1.

Two events can fall on the same clock of the chip, such as a new request and the delivery of an older one. The comment "Order of Events" in [asc.h](asc.h) says what the hardware does in each case.

### Devices of the RP2040

The devices of the RP2040 use registers in memory, from the physical address `0xff0000`: an identification register, the amount of RAM, a clock in microseconds, a console that prints and receives bytes, a disk of 4 KB sectors that the RP2040 copies to and from RAM by itself, a push button, and a screen of 128 by 64 pixels with a frame memory at `0xff1000` and registers that draw characters, rectangles, and pictures ([../asc2-kb/screen-and-button.md](../asc2-kb/screen-and-button.md)). They share line 2 of the interrupt controller.

### Calling Convention

The caller pushes the arguments, the last one first, and removes them after the call. The result comes back in `r0`. A function may change `r0`-`r3` and must restore `r4`-`r7`, `fp`, and `sp`. [../asc2-tcc/Readme.md](../asc2-tcc/Readme.md) has the whole convention, with the rules for 64-bit numbers and structures and the layout of the stack inside a function. A call into the kernel is `int 0x80` with the number of the call in `r0` and the arguments in `r1`-`r5`.

### Firmware Images

A firmware image is a header of three 4-byte numbers (the magic number `0x61736232`, the size of the data section, and the size of the text section) followed by the text section and the data section. The text section becomes the boot ROM, and the data section is copied into RAM at the physical address `0x1000`.

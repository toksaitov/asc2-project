The Machine Contract After Phases 1 to 4 (2026-11-22)
=====================================================

The rules of the machine are in the comments of [asc2-emulated/asc.h](../asc2-emulated/asc.h), and [asc2-emulated/Readme.md](../asc2-emulated/Readme.md) is a map of them. This note does not repeat them. It records which of the open decisions of the plan are made, where the result differs from the wording of the plan and why, and what is still open. The instruction set and the trap and paging rules are frozen since 2026-11-22, after the review of phase 4 was applied (see "What the Review of the Contract Changed"). The compiler and the first kernel have used them.

## Decisions Made

Numbers are those of the reviewer's list.

1. Opcodes, register numbers, and reserved encodings. Bit 7 of the opcode selects the form with an immediate value, so one mnemonic has one opcode in two forms. The 4-bit register numbers 13-15 name nothing. A field that an instruction does not use must be zero. The third register of `dot4` is in the low 4 bits of the immediate field.
2. An immediate value is 16 bits with a sign everywhere. A jump or a `call` to a label counts in instructions from the jump itself. A shift uses the low 5 bits of its count. There are ten conditions. A write to `flags` changes C, Z, S, and O only.
3. After a reset the CPU runs from `0x80800000`, the start of the boot ROM in the kernel's window, in kernel mode with paging off, interrupts off, and every line of the interrupt controller masked. The stack pointers are zero: the first lines of a program set one.
4. Control registers: `cr0` switches (paging, the kernel reads the tables, full fetch address, trap record protected), `cr1` saved `ip`, `cr2` fault address, `cr3` top page table, `cr4` saved `flags`, `cr5` cause, `cr6` the user stack pointer, `cr7` loads a translation.
5. Vectors 0-6 are the exceptions; `int n` is the vector 6 with `n` in bits 15-8 of the cause, so the kernel needs one handler for every system call. A device interrupt takes the first vector of the interrupt controller plus its line, 8-15 after a reset. The saved `ip` is the instruction itself for an exception, the next one for `int`, and the next one to run for an interrupt.
6. The cause of a page fault tells if the entry was present, if the access was a write, a fetch, or made in user mode, which table stopped it, if an entry was malformed, and the status of the RP2040 if the read of an entry was refused.
7. An instruction that raises an exception has changed no register. The emulator does every check before the first change. Phase 4 limited the rule to what the chip can know; see below.
8. The keyboard controller has a status port, `0x64`, beside its data port `0x60`: bit 0 says that a byte waits, bit 1 that a byte was lost. A read of the data port clears bit 0, and a write of bit 1 to the status port clears bit 1 (changed in phase 4). The time-out of a broken byte from the keyboard is in clock cycles and is chosen with the hardware description.
10. ELF: machine number `0xa5c2`, relocation sections with addends, four relocation types (a 4-byte address, the lower and upper half of an address, the distance of a jump).

## Where the Result Differs From the Plan's Wording

* The upper half of a 32-bit value is loaded by its own instruction, `lih`, which replaces the upper 16 bits and keeps the lower 16. The plan said "a load of the upper half and an OR of the lower half". An OR with a 16-bit value that has a sign would set the upper half when bit 15 is set, and `lih` changes no flags.
* An interrupt that wakes a halted CPU returns to the instruction after `hlt`. Otherwise every handler would have to move the saved address.
* `cr7`: a translation of the same page is replaced in place, so a page never has two remembered translations. A value without the present bit is ignored, and so are its bits 31-24. The plan's rule stands: nothing removes one translation.
* The keyboard controller of the emulator takes whole bytes. The receiver of the keyboard's serial line is hardware only.
* A disk command takes time (16 steps of the emulator), and writes to the disk's registers are ignored while it runs. A disk that answers at once would hide the mistakes that a kernel makes with a slow one.
* A test passes with `0x600d` in `r0`, not with zero. Empty memory is full of `hlt`, and a mistaken jump into it would have passed.
* The assembler reads hexadecimal, binary, and decimal numbers, refuses a label that is defined twice, and has `.ascii` and `.asciz`.
* The display controller is gone from the emulator. Its hardware description and the VGA code go in phase 5, with the rest of the Almurut I hardware description.
* The 8-bit example programs (`pong`, `ball`, `irqcheck`) are removed: they cannot be assembled for the new machine. [hello.s](../asc2-firmware/hello.s) takes their place.
* The emulator on the screen sends the keys to the console. With `-k` it sends the character code of a key to the keyboard controller; it does not translate to the codes of a real keyboard.

## What the C Compiler Decided (Phase 2)

The compiler is TinyCC 0.9.27 with a code generator for the machine, in [asc2-tcc/](../asc2-tcc/). Its [Readme.md](../Readme.md) is the home of the calling convention and of the limits below.

* The instruction set did not change. The code generator found an instruction for every step that TinyCC asks for, and constants and addresses load without a change of the flags (`li`, `lih`), which TinyCC needs between a comparison and the jump that uses it.
* The relocation types did not change. TinyCC 0.9.27 writes relocation sections with addends for a 32-bit target after one selection in `tcc.h` and three places in `tccgen.c`, so the object files of the compiler and of the assembler have the same form and link together.
* The calling convention of phase 1 stays, and C adds these rules: each argument takes whole 4-byte words; a 64-bit value is two words or `r0` and `r1`, lower half first; a structure is copied onto the stack as an argument and is returned through a hidden first argument; a result shorter than 32 bits comes back extended; `r4`-`r7` are kept by a function, and the compiler does not use them at all.
* `char` has a sign. `int`, `long`, and pointers are 4 bytes, `long long` is 8 bytes and starts at a multiple of 8 inside a structure.
* There is no floating point and no instruction for a division. The runtime library divides in software and answers a division by zero with a quotient of all ones and the dividend as the remainder.
* One function has up to 32 KB of local variables, and a jump or a call reaches 32767 instructions (128 KB of code) in either direction. Both limits come from the 16-bit field.
* A linked program is an ELF file. The linker got the option `-Wl,-Tdata=` for the address of the variables, and [asc2-tools/elf2fw/](../asc2-tools/elf2fw/) turns the file into a firmware image.

## What the Kernel Decided (Phase 3)

The kernel is in [asc2-rach2/](../asc2-rach2/), and its [Readme.md](../Readme.md) is the home of its design. It boots on the emulator and runs five processes with their own page tables, time sharing by the timer, stacks that grow by page faults, and four system calls.

* The trap rules, the control registers, and the paging rules did not change. One entry point serves every vector, because the cause register tells them apart. The entry writes nothing to memory, so the kernel chooses the form of its own record of a trap. The bit that protects the trap record is cleared by the kernel once the record is on its stack and set again before the record is loaded back.
* The kernel needs no page tables of its own: it lives in the kernel's window, which is the same with paging on and off. A change of the process is one write to `cr3`.
* Three changes of the machine, all outside the chip:
    * The boot ROM range is 256 KB, not 16 KB. The bytes after an image are zeros.
    * A read-only register of the RP2040 at `0xff0008` holds the amount of RAM in bytes. The kernel reads it and has no number of its own for the size of RAM.
    * RAM other than the variables of an image is not zero after a reset of the real machine, and a program must not count on zeros there. The runner of the tests fills that RAM with `0xff` (`-r`), and every suite runs so.
* The assembler got `.incbin`, which puts the bytes of a file into a section. The kernel carries its first programs that way until it has a disk.
* The link addresses: the kernel as a firmware image, with its code at `0x80800000` and its variables at `0x80001000`; a program at `0x00400000`, the address that the linker of TinyCC chooses by itself. The stack of a program ends at `0x00800000` and may reach 64 KB. The page at the address 0 is never mapped, so a NULL pointer faults.
* A system call is `int 0x80` with the number in `r0`, the arguments from `r1` on, and the result in `r0`. The numbers and the error numbers are those of Linux for the 32-bit x86.
* Costs, measured on the emulator:
    * The first boot with its five processes takes 3.2 million instructions.
    * To clear or to copy a page takes about 12,400 instructions with the C functions of the runtime library, and about 70% of the boot went there. The same loops in assembly would take a third or a quarter of that.
    * The image of the kernel has 25 KB with its two programs and the 3.7 KB of the runtime library. 600 lines of C give about 10 KB of code, so a kernel with a file system and `fork` and `exec` may need about 85 KB of ROM.
    * 46 of the 48 pages of RAM are free after the start of the kernel, and the five processes use up to 42 of them at once.

## What the Review of the Contract Changed (Phase 4)

The review asked for seven changes, three of them before any freeze. All seven are made. The rules are in [asc.h](../asc2-emulated/asc.h); this is the list of what changed and why.

* Sequential fetch has a rule (the comment at `cr0`). The emulator used to translate every fetch and read the right word whatever the chip would have asked the RP2040 for. Now the emulated RP2040 keeps its own fetch pointer, the CPU asks for "the next word" under the rule, and a wrong rule runs a wrong instruction. A run of such fetches ends at a trap, at any write to `ip`, at any write to a control register, at a page table lookup that has loaded a translation, and at the last word of a page.
    * The reviewer named `cr0`, `cr3`, and `cr7`. The rule says "any control register" because the chip then needs no comparison of the register number, and the cost is one address after a rare instruction.
    * A write to `cr3` alone would not need the rule: the CPU forgets its translations, and the lookup or the miss that follows ends the run anyway. A planted mistake showed that.
    * A reset sets the F bit (every fetch with its address), and boot code clears it. The kernel's [loader.s](../asc2-rach2/loader.s) and the start-up code of the C tests do. A chip whose sequential fetch is broken still boots, and its kernel leaves three instructions out.
    * Measured on the boot of the kernel: 3,223,066 fetches, 2,735,792 of them (85 percent) without an address. With the 1,475,618 reads and writes that is 19 percent fewer bytes on the bus.
* The restart rule is limited to what the chip knows. The registers are untouched after an exception. A translation that a lookup has loaded stays. After the bus status 5 nobody knows whether the RP2040 has done the access, and the emulator shows the case in which it has. A register of the RP2040 at `0xff0030` orders a lost answer for a test; the firmware gets the same register in phase 7.
* A failed read of a handler's address stops the CPU with the record of the exception that was on its way.
* A page table must lie in the first 8 MB of physical memory, where RAM is. The chip refuses a table address above that, where the boot ROM and the device registers are, without reading there (a malformed entry, the M bit of the cause). The RP2040 answers for the addresses below 8 MB that hold no RAM. (Until 2026-11-23 the rule said "the lower half" of the 16 MB; the border is the same.)
* The example of software refill takes the permissions of both tables, as the chip's own lookup does. The kernel's copy to and from a program checks both tables, and its page fault handler grows the stack only for an absent entry.
* The lost-byte bit of the keyboard controller stays until it is written. A driver that reads the status and then the data would otherwise miss a byte lost between its two reads.
* `int` has a line of its own in the list of finished instructions, with the handler as its target. It completes once, as any other instruction.
* A fetch from a device register is answered with the status 2, as the firmware of the bench does. The emulator said 1.
* The order of two events on the same clock is written down ("Order of Events" in [asc.h](../asc2-emulated/asc.h)): interrupts are taken between instructions only, a request that arrives on the clock of a delivery stays, the line of the RP2040 is a level, a write of the timer's low byte takes the place of the tick of that clock. The emulator cannot place two events on one clock, so these are rules for the hardware description and its tests.
* A request that arrives between `sti` and `hlt` is taken before the `hlt`, and the `hlt` waits for another one. The CPU does not hold interrupts back for one instruction after `sti`, as x86 does. The disk test slept forever when its disk was fast enough; it now keeps the timer coming and sleeps in a loop, as the kernel's idle loop does.

Two questions of the earlier phases are answered:

* No instruction that loads the upper half of a register and clears the lower one. The reviewer counted its places in the boot of the kernel: 4,638 of 3,219,508 finished instructions, 0.14 percent.
* The timer keeps running from the reset with the longest period. A first interrupt right after the kernel opens the line is correct.

The reviewer also listed where a chip is most likely to differ from the emulator. The cheap ones are tests now: every bus request in order across a page boundary and a change of the tables, the whole record at a double fault, `call lr`, an addition into `flags`, a bad instruction word in user mode, and `iret` to a bad address. The ones that need a clock (two events at once, keyboard bytes that are zero, repeated, or broken) are listed in [asc2-tests/Coverage.md](../asc2-tests/Coverage.md) and wait for the hardware description. For the kernel of phase 8 the review notes: `exec` needs a defined stack of arguments and checks of the program header, and the disk driver shares its interrupt line with the console.

## 32-Bit Physical Addresses (2026-11-23)

Since 2026-11-23 the machine has 4 GB of physical addresses in place of 16 MB. This changed frozen rules. [asc.h](../asc2-emulated/asc.h) has the rules as they are now. What changed, and why each point was decided so:

* A physical address has 32 bits. A page table entry and `cr3` hold a page number of 20 bits, and so does a translation that the kernel loads through `cr7`. A present entry has no bits that must be zero any more.
* The window of the kernel is the whole upper half of the addresses and shows the lower half of physical memory: the chip clears the top bit of the address. This is the rule of MIPS. The chip lost two checks by it. With paging off, the lower half of the addresses is the lower half of physical memory in both modes. The upper half of physical memory is reached through a page table only.
* The chip refuses no address by its range any more, except an address of the window in user mode. Where the RP2040 serves nothing, it answers with a bus error, and the cause then carries the status 1 of the RP2040.
* Nothing moved. RAM is at 0, the boot ROM at `0x00800000`, the device registers at `0x00ff0000`, the reset address is `0x80800000`, and the kernel and every program run unchanged. Moving the devices to the end of the window would be tidier and would change the kernel, the firmware, and every test a week before the shuttle closes.
* A page table lies in the first 8 MB, as before: below the boot ROM and the device registers, so the chip never reads a device register as a table. The check is nine bits wide in place of one.
* The bus sends four address bytes with every request that has an address. One form of the frame is easier to test than two; the cost is one byte more on such a request.
* The RP2040 serves 1 KB of RAM above 16 MB, at the start of the last page of the 4 GB, so that the tests can tell an address from the same address without its upper byte.

What it costs is in [tt-flow.md](tt-flow.md), section 15.

## Open

* The RP2040 keeps the boot ROM in its own RAM, as the bench has shown, not in its flash. ROM and RAM of the machine so share one supply: about 208 KB on the RP2040 board of the bench and about 400 KB on an RP2350 board. A kernel of about 85 KB would leave about 30 pages of RAM on the RP2040. How the firmware of the RP2040 divides its memory, and whether it can serve the ROM from its flash after all, is decided in phase 7. The bench moved the ROM out of the flash because a read of the flash can stall for 55 ms while a sector of the disk is written (found on the bench). An earlier version of this note said that a ROM in the flash costs no RAM; that overlooked this finding.
* `rp_disk` counts on a disk command that is still running six bus accesses after its start. A firmware that copies a sector at once would fail it; phase 7 gives a command a shortest time or changes the test.
* The period of the timer (`PIT_PERIOD` of the kernel, `0x8000` now) is chosen on the bench, where the speed of the bus is known.
* Whether the firmware of the RP2040 clears RAM at a reset is decided in phase 7. Programs do not count on it.
* The compiler puts string literals and constants among the variables, so they take RAM, not ROM.
* The image of the kernel carries its test programs until the disk exists (phase 8).
* The runtime library is one object file, so every program carries all of it (3.7 KB). It can be split when the size matters.
* The tests that depend on time (the timer, the keyboard bytes that arrive after a number of instructions) were written to tolerate other speeds, and they have not run on a hardware description.

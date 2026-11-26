The Design of the System (2026-11-23)
=====================================

This note records how the kernel grows from its first boot to a system with a shell, and why. It is the outline of phase 8 of the plan of milestone 1, written before the code and corrected afterwards where the code came out differently.

## What Is There Already

The kernel of phase 3 ([asc2-rach2/](../asc2-rach2/), 1,900 lines): the trap entry, the timer, paging, processes with their own page tables, a round-robin scheduler, a loader of ELF files, and four system calls. Its programs are part of its image, it has no disk, and it reads nothing from the console.

Rach2's first milestone (commit `ae5d05362e` of the Rach2 repository) has what is missing, for an x86 PC: `ish`, the utilities, the system calls they need, and KRFFS in a RAM disk. Its interfaces and algorithms are taken; the code is new, because the machine is different in three ways: the disk gives one sector at a time and RAM has 48 pages, so the disk cannot be in memory; the CPU refuses a 4-byte access at an odd address, so a packed structure cannot be read in place; and the machine is slow.

## How Slow the Machine Is

On the bench the Pico answers about 90,000 bus transactions a second, and an instruction is one or two transactions. So the machine runs about 60,000 instructions a second, whatever the clock of the chip is. The design follows from that in four places: a disk sector goes between the flash and RAM inside the Pico and costs the CPU a few transactions; a page that will be overwritten at once is not cleared first; the kernel does not look at the console while nobody reads; and a read of the disk is waited for in a short loop, because the Pico finishes it before the CPU can ask twice.

## What the Measurements Changed

The first build of the system took 263,000 instructions for one command of the shell, which is more than four seconds on the bench. A count of the instructions by function (the runner prints the address of every instruction) showed where they went, and four changes brought a command to 121,000 instructions, about two seconds. The start of the kernel with the shell takes 156,000.

* The compiler makes a loop over words of 13 to 18 instructions a word. `memset` and `memcpy` are in assembly now ([asc2-tcc/lib/memory_asm.s](../asc2-tcc/lib/memory_asm.s)): 4 and 6 instructions a word, and 6 a byte when the two addresses do not allow words.
* The machine has no instruction for division, and the compiler calls a function for [/](./) and `%` even when the divisor is a power of two. The kernel shifts and masks where it counts: in the cache of the disk and in the walk over a page table.
* `fork` and `exit` walk the 1,024 entries of a page table of which a handful are in use. They now pass four empty entries at a time.
* A kernel stack is written before it is read, so a new one is not cleared.

What still costs: the bytes of a file start at an odd place of the disk, after a header of 357 bytes, so a program is copied from the cache to its pages byte by byte. A file system that puts the bytes at a multiple of four, or a copy that shifts words, would make `exec` faster. This is left for pass two, with copy on write.

## Processes

* A process is ready, running, sleeping, or a zombie. A sleeping process waits for something that an interrupt or another process brings: a line from the console, the end of a disk write, the end of a child, or its turn in the file system.
* `sleep(channel)` leaves the CPU until `wakeup(channel)`. A channel is the address of the thing waited for. The kernel runs with interrupts off, so nothing can come between the check of a condition and the sleep, and no wakeup is lost. A woken process checks its condition again.
* When no process is ready and some sleep, the scheduler halts the CPU with interrupts on. The next interrupt wakes it. When no process is left at all, the machine stops with the result of the first process, as before.
* `fork` makes a child with a copy of every page of the parent and of its open files. The child gets 0 from the call. Sharing the pages until one side writes (copy on write) needs no change of the chip and is left for pass two.
* `exit` closes the files, frees the memory, and leaves a zombie with the exit code for the parent. `waitpid` sleeps until the child is a zombie and frees its entry. The children of a process that ends have nobody to wait for them: a zombie among them is freed at once, and the others are freed when they end.
* A process that the kernel stops for a mistake ends like a process killed by a signal on Linux: the status that `waitpid` gives has the number 11 in its low bits.

## Starting a Program

`execve(path, argv, envp)` builds the new memory next to the old one and switches only when everything is ready, so a failure returns to the old program with an error.

* The file is read from the disk piece by piece straight into the new pages: the header, the program headers, then every part that must be loaded. No copy of the whole file exists in the kernel.
* The checks of the file: the ELF identity, 32 bits, low byte first, an executable for this machine; at most 8 program headers of the right size inside the file; every part inside the file and between the first page of a program and its stack; the first instruction inside a loaded part.
* The arguments and the environment are copied from the old memory to the top page of the new stack. All of them must fit in that page; at most 32 arguments and 16 environment strings.
* A new program starts with the stack pointer at a word with the number of arguments. The addresses of the argument strings follow, then a zero, the addresses of the environment strings, and a zero. The strings are above. The start-up code of a program ([user/crt0.s](../asc2-rach2/user/crt0.s)) passes the three values to `main`.
* The open files stay open.

The first process is the program `ish` on the disk, with the arguments `ish` and the environment `HOME=/` and `PATH=/`. A disk without a file system or without `ish` stops the kernel with a message.

## The Console

* Output is as before: a byte is written when the console can take it.
* Input is read a line at a time. A process that reads sleeps until a line is complete. Only then does the kernel allow the console's interrupt and take bytes. It echoes them, lets Backspace remove the last one, and ends the line at Enter. `ctrl+d` on an empty line ends the input: the read returns 0.
* What is typed before a program reads stays in the Pico. So the output of a session is the same at any speed of typing, and a test can give its whole input at once.
* The console and the disk share interrupt line 2 of the chip. The handler asks the interrupt status register of the RP2040 which of them it was.

## The Disk

* The driver reads and writes one sector of 4 KB. It gives the RP2040 the sector's number and the address of a buffer in RAM and starts the command.
* A read is waited for in a loop. A write to the flash takes tens of milliseconds, so the process sleeps until the disk's interrupt, and other processes run meanwhile. Before the first process exists the kernel waits in a loop in both cases.
* The contract has no register with the size of the disk. The kernel finds it at the start: it asks for sectors with doubled numbers until the disk refuses one, and then halves the gap.
* A cache of two sectors stands between the file system and the driver. It gives bytes at any position of the disk, also across the border of two sectors. A changed sector is written when its place is needed and at the end of every system call that changed the disk. So the disk is whole whenever the shell waits for a command, and a reset there loses nothing.

## The File System

KRFFS is the file system of Rach2: the K&R storage allocator on a disk. The disk is one ring of nodes. A node is a header and the bytes of a file after it. A header has the type (the root, free space, or a file), the name with its hash, and three sizes: of the node, of the node before it, and of the data in use. A new file takes the first free node that is large enough, and the rest of that node stays free. A removed file becomes free space and joins the free nodes next to it. A file that grows takes space from a free node after it, or moves to a free node that is large enough.

The bytes of a file are in one piece. So a disk that has no room for a large file may still have smaller free pieces between the files, and a write that does not fit takes the bytes that fit.

* The format is the one of Rach2's present host tools, byte for byte: a packed header of 357 bytes, with all files in the root (`parent_id` 0). So `mkfs.krffs`, `fsck.krffs`, and the other tools of Rach2 work on a disk of Almurut II, and `fsck.krffs` is an independent check of what the kernel writes. Directories, links, and owners are not used.
* The kernel never has a header in place. It reads the fields that it needs through the cache into a small structure (position, type, hash, the three sizes) and writes them back the same way. The 64-bit sizes of the format must fit in 32 bits.
* A file is found by the hash of its name; the name is compared after the hash.
* A path may start with [/](./). The names [/](./) and `.` are the root, which can be opened to list the files. Any other [/](./) in a name is refused.
* One process at a time is in the file system. The others sleep until it leaves.
* An open file is remembered by the position and the hash of its node. A file may move when it grows, so every use checks the node at the position and looks the file up again if it is not there. A file that was removed while it was open gives an error. Linux would keep it until it is closed.
* The times in a node are the seconds since the kernel started. The machine has no clock that keeps the date.

## System Calls

The numbers and the meanings are those of Linux for the i386, as in Rach2: `exit` 1, `fork` 2, `read` 3, `write` 4, `open` 5, `close` 6, `waitpid` 7, `creat` 8, `unlink` 10, `execve` 11, `chdir` 12, `lseek` 19, `getpid` 20, `rename` 38, `times` 43, `dup2` 63, `stat` 106, `getdents64` 220, and `utimensat` 320. An error is a negative Linux error number.

* `open` knows reading, writing, both, `O_CREAT`, `O_TRUNC`, `O_APPEND`, and `O_DIRECTORY`.
* `chdir` accepts the root and nothing else.
* `stat` fills the structure of Linux for the i386 with the mode, the size, and the times.
* `utimensat` does what `touch` asks: it sets the times of an open file to now.
* A process has 8 file descriptors. Each is the console, a file, or the root directory. `fork` copies them, positions included.

## The Programs

* [asc2-rach2/user/](../asc2-rach2/user/) has what every program is linked with: the start-up code and the system calls as C functions ([user.h](../asc2-rach2/user/user.h), [syscalls.s](../asc2-rach2/user/syscalls.s)).
* [asc2-rach2/ish/](../asc2-rach2/ish/) is `ish` of Rach2 in its own style, with these changes: the system calls come from [user.h](../asc2-rach2/user/user.h); it prints a prompt when its input is the console; the redirection files are opened in the child, so the shell does not keep them; `<` and `>` may stand anywhere on the line, and the last one of a kind counts; `exit` without a number ends the shell with the status of the last command. A name that is no program gives the status 127, a file that cannot run 126, and a child that the kernel has stopped 128 plus the number of the reason, as in the shells of Linux.
* [asc2-rach2/utils/](../asc2-rach2/utils/) has `echo`, `cat`, `ls`, `cp`, `mv`, `rm`, and `touch`. `ls` prints the size and the name of every file.
* [asc2-rach2/tests/](../asc2-rach2/tests/) has the programs that check the kernel from the side of a process. They are files on the test disk.
* `asc2-tools/krffs/asc_krffs` builds a disk image from files on the Mac, lists an image, takes a file out of it, and checks its structure.

## Memory

RAM has 48 pages. The kernel takes page 0 (the vector table and the boot stack) and 3 pages of variables: the table of 8 processes with their file descriptors, the two sectors of the cache, and the line of the console. 44 pages are free after the start. A process takes a top table, one second-level table, a kernel stack, and the pages of its program and its stack: 7 for `ish`. The largest need is a `fork` followed by `exec`: the shell, the child's copy of the shell, and the new program before the copy is freed. The kernel counts the least number of free pages that it has seen and prints it at the end: 24 after a session of single commands, so a command needs 20 pages at its peak. The deepest use of a kernel stack that was seen is 1,160 of its 4,096 bytes.

The image of the kernel must fit in the 48 KB that the RP2040 of the bench leaves for the boot ROM. It has 36 KB.

## Checking

* `make kernel-suite` builds the kernel, the programs, and a disk image, and boots the kernel on the emulator three times with a file of typed lines: the shell and the utilities, a second boot from the disk that the first has left, and the test programs. It compares the console output with a file and checks every disk that a run has left: the structure with `asc_krffs`, the listing of `ls` against the list of `asc_krffs`, and a copy that the session made against the file.
* The sessions run on the chip in Verilator, step by step with the emulator and on the chip's own time, and on the board.
* On the bench the disk image goes into the flash of the Pico with `picotool`, and comes back the same way. The monitor of the bridge tells where the disk is.
* When the kernel stops, it prints the exit code of the first process. The board has no other way to show it.

[asc2-tests/Readme.md](../asc2-tests/Readme.md) describes the sessions.

## Open Points

* Copy on write for `fork`, and a faster load of a program (pass two).
* `execve` clears every new page before it reads the file into it. Only the part that the file does not fill needs it.
* `fork` fails with `EAGAIN` when the table of processes is full. No test reaches it: RAM is full before the eighth process.
* No signal exists, so nothing typed stops a program that runs away. The monitor's `run` restarts the machine.
* A run of the test programs on the board erases the sector with the header of one growing file about a hundred times. The flash of a Pico is good for about a hundred thousand.

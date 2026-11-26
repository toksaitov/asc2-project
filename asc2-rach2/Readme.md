Rach2 for Almurut II
====================

This directory contains the kernel of the Almurut II computer and the programs of its disk. The kernel is a small relative of Rach2, written anew for this machine with the names and the structure of Rach2. The overview of the whole platform is in [../Readme.md](../Readme.md).

The kernel runs programs as processes. Each process has its own memory behind page tables, and the timer takes the CPU from one process and gives it to the next. A process makes a child with `fork`, and `execve` gives a process a program from the disk. The disk has the file system of Rach2, KRFFS. A program asks the kernel for services with system calls that have the numbers of Linux.

The first process is the shell `ish`. The disk also has eight utilities: `echo`, `cat`, `ls`, `cp`, `mv`, `rm`, `touch`, and the line editor `ed`. The shell greets a person at the console with the file [motd](motd). The disk of the machine has the programs of the showcase too (see The Showcase). [../asc2-kb/system-design.md](../asc2-kb/system-design.md) records why the kernel is built as it is.

## Layout

* [loader.s](loader.s): the first instructions after a reset: the stack, the switch to sequential fetch, and zeros in the variables
* [kernel.c](kernel.c): `kernel_main`, which prepares the parts of the kernel and starts `ish`
* [sys/machine.h](sys/machine.h): the numbers of the machine that the kernel uses
* [sys/cpu.s](sys/cpu.s): the IO ports, the control registers, and `hlt` as C functions
* [sys/isr_handler.s](sys/isr_handler.s), [sys/isr.c](sys/isr.c): the way into the kernel for exceptions, interrupts, and system calls, and back
* [sys/pic.c](sys/pic.c), [sys/pit.c](sys/pit.c): the interrupt controller and the timer of the chip
* [sys/rp.c](sys/rp.c): the interrupt line that the devices of the RP2040 share
* [sys/console.c](sys/console.c), [sys/kio.c](sys/kio.c), [sys/panic.c](sys/panic.c): the console of the RP2040 with its typed lines, the messages of the kernel, and its last message
* [sys/disk.c](sys/disk.c): the disk of the RP2040 and a cache of two sectors
* [sys/fs/krffs.c](sys/fs/krffs.c), [sys/fs/krffs_format.h](sys/fs/krffs_format.h): the file system and the numbers of its format
* [sys/fs/file.c](sys/fs/file.c): the open files of a process and the system calls for files
* [sys/syscall.c](sys/syscall.c): the table of the system calls and the calls for processes
* [sys/mm/pages.c](sys/mm/pages.c): the free pages of RAM
* [sys/mm/paging.c](sys/mm/paging.c): page tables, the memory of a program, and page faults
* [sys/proc/process.c](sys/proc/process.c): processes, `fork`, `exit`, and the wait for a child
* [sys/proc/exec.c](sys/proc/exec.c): the loading of a program from its ELF file on the disk
* [sys/proc/sched.c](sys/proc/sched.c), [sys/proc/switch.s](sys/proc/switch.s): the scheduler, sleeping and waking, and the change from one kernel stack to another
* [sys/proc/slice.c](sys/proc/slice.c): the length of a turn, chosen by a small network with the instruction `dot4`
* [sys/proc/array.c](sys/proc/array.c): the registers of the array of the dot products, a set for every process
* [sys/lib/](sys/lib/): integer types and functions for strings
* [user/](user/): what every program is linked with: the start-up code ([crt0.s](user/crt0.s)), the system calls as C functions ([syscalls.s](user/syscalls.s), [user.h](user/user.h)), and a few functions for text ([text.c](user/text.c))
* [ish/](ish/): the shell, taken from Rach2 in the style of its own code
* [utils/](utils/): the utilities
* [tests/](tests/): programs that check the kernel from the side of a process

## Building and Running

```bash
make
```

This builds the assembler, the C compiler, and the tools for images if needed, and then:

* `build/rach2.bin`: the kernel, a firmware image for the boot ROM
* `build/bin/`: the shell and the utilities, as ELF files
* `build/disk.img`: a disk of 256 KB with these programs
* `build/tests/disk.img`: the same disk with the programs of [tests/](tests/) too

`make clean` removes `build/`. To run the system in the emulator:

```bash
../asc2-emulated/asc -d build/disk.img build/rach2.bin
```

The emulator writes the disk file back when it exits, so the files that were made in a session are there in the next one. `../asc2-tools/krffs/asc_krffs` lists and checks a disk image and takes files out of it.

To put the system on the bench:

```bash
make install
```

This writes the disk and the kernel into the flash of the Pico. The Pico then starts the kernel by itself at every power-up; any serial terminal on its port is the console, and the display of the bench shows the console too.

`cd ../asc2-tests && make kernel-suite` runs the kernel without a screen, types three sessions to it, and compares the console output with the files in [../asc2-tests/kernel/](../asc2-tests/kernel/). [../asc2-bridge/Readme.md](../asc2-bridge/Readme.md) tells how to run the system on the board.

## The Shell

`ish` prints `$ ` and reads a line: a command and its arguments, which spaces separate. `< file` gives the command a file as its input, and `> file` sends its output to a file. They may stand anywhere on the line, and the last one of a kind counts.

* A command is the name of a program on the disk. `ish` makes a child, the child becomes the program, and `ish` waits for its end.
* `cd` and `exit` are parts of the shell. `exit` ends it with the number that follows, or with the exit code of the last command.
* The end of the input, `ctrl+d` on an empty line, ends the shell too.

A command that reads the console gets what is typed, a line at a time, until `ctrl+d` on an empty line:

```
$ cat > notes.txt
a line of text
$ cat notes.txt
a line of text
$ ls notes.txt
      15 notes.txt
```

## Memory

RAM has 192 KB, which is 48 pages. The kernel runs in the kernel's window of the address space, `0x80000000`-`0xffffffff`, where an address is a physical address plus `0x80000000` whether paging is on or off. It needs no page tables of its own.

* `0x80000000`: the interrupt vector table, 1 KB. The rest of this page is the boot stack.
* `0x80001000`: the variables of the kernel, about three pages.
* After the variables: the pages that the kernel gives out for page tables, kernel stacks, and the memory of programs. A register of the RP2040 tells where RAM ends. 44 pages are free after the start.
* `0x80800000`: the code of the kernel, in the boot ROM.

A program has the lower half of the address space. The linker places it at 4 MB, and its stack ends at 8 MB, grows down, and may reach 64 KB. A page of the stack is added when the program first touches it, and only for a page fault that names an absent entry: a malformed entry or a table that could not be read stops the process. The page at the address 0 is never mapped.

A process takes a page for its top table, one for a second-level table, one for its kernel stack, and the pages of its program and its stack. `ish` runs in 7 pages. The largest need is a command: the shell, the child's copy of the shell, and the new program before the copy is freed. The kernel counts the least number of free pages that it has seen and prints it when it stops: 26 after a session of single commands. The copy of the shell takes five pages and not seven, because it shares the two pages of the shell's code.

## How a Program Enters the Kernel

Every entry of the interrupt vector table names `isr_entry`. An exception, a device interrupt, and `int` arrive there, and the cause register tells them apart. The CPU writes nothing to memory on the way: it leaves the address to continue at, the flags, the cause, and the fault address in control registers. `isr_entry` saves these and the registers on the kernel stack of the process as an `isr_params_t`, and `isr_router` calls the function that is registered for the vector. `isr_exit` loads everything back and returns with `iret`. The kernel runs with interrupts off, so it is never interrupted itself.

## Processes

A process has page tables, one page of kernel stack, a table of open files, and a state: ready, running, sleeping, or ended. The kernel has room for 8 processes.

The scheduler is a loop on the boot stack. It chooses the next ready process in the order of the process table, loads the page tables of the process, and switches to the kernel stack of the process with `switch_context`. When the timer interrupts have used up the turn of a process, the handler calls `yield`, which switches back to the scheduler. When no process is ready, the scheduler halts the CPU until the next interrupt.

A turn is long, four periods of the timer, or short, one period. Before every turn a network of one neuron guesses whether the process will go to sleep before the turn ends ([sys/proc/slice.c](sys/proc/slice.c)). A process that is expected to sleep gets the long turn, and it gives the CPU back by itself; a process that is expected to compute on gets the short one, so that the others wait less behind it. The inputs of the neuron are four signed bytes about the process: a constant, the timer interrupts and the system calls of its last turn, and how often it has slept lately. The instruction `dot4` multiplies them by four weights and adds the products, and the kernel computes the same sum once more in C and stops if the two differ. After a wrong guess the weights move a little towards the right answer. `cd ../asc2-tests && make measure` runs the program `load` under the network and under a fixed long and a fixed short turn, and prints what each has done; the results are in [../asc2-kb/sched-network.md](../asc2-kb/sched-network.md).

* A process that must wait calls `sleep` with the address of the thing that it waits for: a typed line, the end of a disk write, the end of a child, or its turn in the file system. `wakeup` with the same address makes it ready again. The kernel runs with interrupts off, so nothing comes between the check of a condition and the sleep.
* `fork` copies for the child every page that the parent may write, and its open files. A page that the parent may not write, a page of its code, is not copied: both processes use the one page, and a byte for every page of RAM counts its users ([sys/mm/pages.c](sys/mm/pages.c)). Four processes of one program of three pages of code take nine pages less that way. The child starts where the parent is, with 0 as the result of the call.
* `execve` reads the ELF file from the disk straight into new pages, copies the arguments and the environment to the top of a new stack, and only then frees the old memory. A file that is not a program of this machine leaves the process as it was, with an error.
* `exit` frees the memory of the process and keeps its exit code until the parent asks for it with `waitpid`.
* The chip has one array of the dot products, and its six registers, the weights and the inputs, keep their values from one instruction to the next. Every process has values of its own ([sys/proc/array.c](sys/proc/array.c)). The kernel does not move the registers at every switch. The process whose values are in the chip uses the array freely. The first instruction for the array of any other process enters the kernel, which stores the registers for the old owner and loads those of the process; the bit A of `cr0` is the switch for that. The kernels of the PC keep the registers of the floating-point unit in the same way. A program starts with zeros, and a child of `fork` has the values of its parent. The network of the scheduler uses `dot4`, which needs none of these registers.

The first process and a child of `fork` start through the same code as every other process. The kernel stack of a new process is built by hand with the words that a program leaves there when it enters the kernel, so the first switch to it returns into `isr_exit`, and `isr_exit` starts the program in user mode.

A new program finds the number of its arguments at the stack pointer. The addresses of the argument strings follow, then a zero, the addresses of the environment strings, and a zero. [user/crt0.s](user/crt0.s) passes them to `main`.

The machine stops when the last process has exited. The kernel prints the exit code of the first process, and `r0` then holds `0x600d` if that code is 0.

## The Console, the Disk, and the Files

* A process that reads the console sleeps until a line is typed. The kernel echoes the bytes, Backspace removes the last one, and Enter ends the line. `ctrl+d` on an empty line ends the input: the read returns 0.
* The disk gives and takes one sector of 4 KB. A read is waited for in a loop; a write takes the flash of the RP2040 tens of milliseconds, so the process sleeps until the interrupt of the disk.
* A cache of two sectors gives the file system bytes at any position of the disk. Every changed sector is written before a system call that has changed the disk returns, so the disk is whole whenever the shell waits for a command.
* KRFFS keeps the disk as one ring of nodes: the root, the files, and the free space. A file has its bytes in one piece after its header. A new file takes the first free node that is large enough; a file that grows takes space from the free node after it or moves to a larger one. The header comment of [sys/fs/krffs.c](sys/fs/krffs.c) explains the format, which is the one of Rach2's tools, byte for byte.
* The disk has one directory, [/](./). A name has at most 255 bytes.
* A process has 8 file descriptors. The first three are the console when `ish` starts.

## System Calls

A program runs `int 0x80` with the number of the call in `r0` and the arguments in `r1`, `r2`, and `r3`. The result comes back in `r0`, and every other register keeps its value. A negative result is the number of an error, with the numbers of Linux. [user/user.h](user/user.h) describes every call.

| Number | Call                                                                        | What it does                                                    |
|--------|-----------------------------------------------------------------------------|-----------------------------------------------------------------|
| 1      | `void exit(int status)`                                                     | ends the process                                                |
| 2      | `int fork(void)`                                                            | makes a child, a copy of the process                            |
| 3      | `int read(int fd, void *buf, unsigned int count)`                           | reads a file or a typed line                                    |
| 4      | `int write(int fd, const void *buf, unsigned int count)`                    | writes to a file or prints on the console                       |
| 5      | `int open(const char *path, int flags, int mode)`                           | opens a file or the directory                                   |
| 6      | `int close(int fd)`                                                         | closes a file descriptor                                        |
| 7      | `int waitpid(int pid, int *status, int options)`                            | waits for the end of a child                                    |
| 8      | `int creat(const char *path, int mode)`                                     | opens a file for writing, empty                                 |
| 10     | `int unlink(const char *path)`                                              | removes a file                                                  |
| 11     | `int execve(const char *path, char *const argv[], char *const envp[])`      | gives the process another program                               |
| 12     | `int chdir(const char *path)`                                               | accepts the one directory                                       |
| 19     | `int lseek(int fd, int offset, int whence)`                                 | moves the position of an open file                              |
| 20     | `int getpid(void)`                                                          | returns the number of the process                               |
| 38     | `int rename(const char *old_path, const char *new_path)`                    | gives a file a new name                                         |
| 43     | `int times(struct tms *buf)`                                                | returns the number of timer interrupts since the start          |
| 63     | `int dup2(int old_fd, int new_fd)`                                          | makes a second descriptor of an open file                       |
| 106    | `int stat(const char *path, struct stat *buf)`                              | tells the kind, the size, and the time of a file                |
| 220    | `int getdents64(int fd, void *buf, unsigned int count)`                     | reads entries of the directory                                  |
| 320    | `int utimensat(int dirfd, const char *path, const void *times, int flags)`  | sets the time of an open file to now                            |
| 1000   | `int screen(int request, unsigned int a, unsigned int b)`                   | lends the screen and the button to the process (not a call of Linux) |
| 1001   | `int slices(int request, unsigned int *words)`                              | tells what the neuron of the scheduler has learned, or makes it forget (not a call of Linux) |
| 1002   | `int nap(unsigned int ticks)`                                               | lets the process sleep for a number of timer interrupts (not a call of Linux) |

## The Screen and the Button

The machine has a display of 128 by 64 pixels and one push button, both devices of the RP2040 ([../asc2-kb/screen-and-button.md](../asc2-kb/screen-and-button.md)). The RP2040 shows the console on the display by itself. A program takes the display from the console with `screen(SCREEN_OPEN, 0, 0)`: the kernel puts the device page into the memory of the process at `0x7ef000`, below the stack, and the program draws into the frame memory, uses the registers for characters, rectangles, and pictures, and reads the button and the clock with no further call. `screen(SCREEN_PICTURE, number, address)` tells the RP2040 where in RAM a picture of the program is. One process has the screen at a time (`-EBUSY` for another), a child does not get it, and the console has the display again when the process gives the screen back, ends, is stopped, or becomes another program ([sys/screen.c](sys/screen.c)).

Two programs of the disk use it; their sources are in [../asc2-demos/](../asc2-demos/), which also has the functions that draw ([lib/machine.h](../asc2-demos/lib/machine.h)):

* `burkut`: a game for the one button. A press is a beat of the burkut's wings; the button held for two seconds ends the game.
* `scenes`: text, a picture, and a short film; a press goes on to the next one.

The kernel reads and writes the memory of a program only through `copy_from_user` and `copy_to_user`, which look every address up in the page tables of the process. A wrong address from a program gives the error `EFAULT` and cannot make the kernel fault.

A program that does what the CPU refuses, such as a write to the address 0 or an instruction for the kernel only, is stopped with a message, and the other processes go on. Its parent learns it from `waitpid`, as on Linux after a signal. An exception inside the kernel itself stops the machine with a message.

## The Showcase

The disk of the machine (`build/disk.img`) has the programs that show what the machine can do. Their sources are in [../asc2-demos/](../asc2-demos/), whose Readme tells how they are made. Each starts with a card that says what is to be seen; a press of the button goes on, and a hold of two seconds ends the program.

* `tour`: starts the others one after another with nobody at the button; each one shows itself for about half a minute
* `race`: the same job done with the array of the dot products and with plain code, as two bars
* `pilot`: a neuron starts with nothing and learns to fly the burkut of the game
* `guess`: the chip learns to guess the next press of the button
* `fence`: three neurons learn to keep the sheep in and the wolves out
* `scope`: a filter of twelve numbers cleans a noisy signal
* `sched`: the neuron of the scheduler at work, seen from a program
* `shapes` and `flight`: 3D, as lines and with filled sides
* `film`: a film of a minute, of pictures and 3D models together
* `mark`: a benchmark of four kinds of work with checked answers and a score; 1000 points are the first machine

The disk of the tests (`build/tests/disk.img`) has the shell, the utilities, `burkut`, `scenes`, and the test programs only: the tests need its free room. The sessions of the showcase in the tests run from the disk of the machine.

## What the Kernel Leaves Out

* Signals: nothing that is typed stops a program that runs away.
* Directories other than [/](./), links, and owners of files.
* Copying on the first write: `fork` copies every page that may be written at once. Only the pages of code are shared.
* A file that is removed while a process has it open gives that process an error. Linux keeps such a file until it is closed.
* A child of `fork` has its own position in an open file. On Linux the parent and the child share one.
* The date: the time of a file is the number of seconds since the kernel started.

## The Test Programs

The programs of [tests/](tests/) are files on the disk of the tests. Each prints one line for every part of the kernel that it has found right, or a line that starts with `FAILED`.

* `work`: the variables, the growth of the stack, the registers across timer interrupts and system calls, buffers that lie on two pages, and two processes that share the CPU
* `crash`: five children, each with another mistake that the kernel must stop it for
* `forks`: `fork`, `exit`, and `waitpid`, until the machine has no room for one more process
* `execs`: `execve` with wrong names, wrong arguments, and damaged ELF files, and the arguments and the environment that a program gets
* `args`: prints its arguments and its environment; `execs` and the shell start it
* `files`: the system calls for files, a file that moves while it is open, two processes that write at once, and a disk that fills up
* `dots`: the instruction `dot4` in two processes that the timer switches between, each sum compared with the sum of four multiplications in C
* `array`: the array of the dot products in two processes that the timer switches between, each sum and each weight compared with a model in C; the zeros of a program that starts, and the values that a child gets
* `load`: five processes of three kinds (two compute, two write a file, one does both), the load that the scheduler is measured with
* `screen`: the loan of the screen: who may use the device page, what a picture must be, and that the screen is free again after its process has given it back, ended, or been stopped

## Numbers of the Machine

[sys/machine.h](sys/machine.h) repeats the addresses, ports, vectors, and register bits that the kernel needs from [../asc2-emulated/asc.h](../asc2-emulated/asc.h), with the same names. The build compiles the two files together, and the compiler refuses a name that has two values. Assembly files have no such check: they use the numbers themselves and name them in comments.

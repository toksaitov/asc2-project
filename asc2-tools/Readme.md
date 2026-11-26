Almurut II Tools
================

This directory contains support tools used by the test and build flows. The overview of the whole platform is in [../Readme.md](../Readme.md).

* [reftrace/](reftrace/): `asc_reftrace`, the emulator without a screen. It runs a firmware image until the CPU is parked, prints one line of CPU state for every instruction that has run, and reports the result that the program has left in `r0`. [trace_common.h](reftrace/trace_common.h) holds the format of the lines, the exit statuses, and the code that the runner shares with `asc_trace` in [asc2-tests/chip/](../asc2-tests/chip/), which runs an image on the hardware description of the chip.
* [elf2fw/](elf2fw/): `asc_elf2fw`, which turns a program linked by the C compiler into a firmware image.
* [krffs/](krffs/): `asc_krffs`, which makes a disk image with the file system of the kernel from files of this computer, and lists, reads, and checks such an image.

## Running a Program Without a Screen

```bash
gcc -g -std=c11 -Ireftrace -o reftrace/asc_reftrace reftrace/asc_reftrace.c
reftrace/asc_reftrace [-n max_retired] [-q] [-r] [-e event_file] [-i console_input_file] [-o console_output_file] [-d disk_file] [-s disk_output_file] [-b bus_log_file] path_to_binary
```

* `-n` sets the number of instructions after which the tool gives up (100000 without it).
* `-q` prints no lines of CPU state.
* `-r` fills the RAM around the variables of the program with the byte `0xff` before the start. The RAM of the real machine is not zero after a reset, and a program that counts on zeros there fails with this option.
* `-e` names a file with the bytes that the keyboard sends.
* `-i` names a file with the bytes that the console receives.
* `-o` names a file for what the program prints on the console.
* `-d` names a file with the contents of the disk. The file is not changed.
* `-s` names a file for the contents of the disk at the end of the run.
* `-b` names a file for the requests that the CPU sends on the bus, one per line: a fetch with its address, a fetch of "the next word", a read, or a write, with the mode, the size, the address, the value, and the answer.

When the CPU stops with a double fault, the tool prints the address of the instruction and the trap record. [../asc2-tests/Readme.md](../asc2-tests/Readme.md) describes the exit statuses and the companion files, and [../asc2-tests/Makefile](../asc2-tests/Makefile) builds and runs the tool for the test programs.

## Making a Firmware Image From a Linked Program

```bash
make -C elf2fw
elf2fw/asc_elf2fw path_to_program output_file
```

The program is an ELF file that the linker of [../asc2-tcc/](../asc2-tcc/) has written with its code at `0x80800000` and its variables at `0x80001000`, the two addresses of a firmware image. The tool refuses a program that is linked elsewhere, that does not start at the first instruction of its code, or that is too large for the boot ROM or the RAM. [../asc2-tcc/Readme.md](../asc2-tcc/Readme.md) shows the commands that lead to such a program.

## Making and Reading a Disk Image

```bash
make -C krffs
krffs/asc_krffs make image size_in_kb [file...]
krffs/asc_krffs list image
krffs/asc_krffs get image name output_file
krffs/asc_krffs check image
```

* `make` writes a new image with the given files. A file keeps its name without its directories and the permissions that it has on this computer.
* `list` prints the size and the name of every file, as `ls` of the machine does.
* `get` copies a file of the image into a file of this computer.
* `check` prints the nodes of the image and ends with an error if the file system is not sound: a node that does not follow the one before it, sizes that do not fit, a name that does not go with its number, or two free nodes side by side.

The format is the one of Rach2's host tools, and the numbers of it come from [../asc2-rach2/sys/fs/krffs_format.h](../asc2-rach2/sys/fs/krffs_format.h), the header of the kernel. [../asc2-rach2/Makefile](../asc2-rach2/Makefile) makes the disks of the machine with this tool, and [../asc2-tests/Makefile](../asc2-tests/Makefile) checks with it what the kernel has left on a disk.
